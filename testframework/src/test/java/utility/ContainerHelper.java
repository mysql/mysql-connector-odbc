// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0
// (GPLv2), as published by the Free Software Foundation, with the
// following additional permissions:
//
// This program is distributed with certain software that is licensed
// under separate terms, as designated in a particular file or component
// or in the license documentation. Without limiting your rights under
// the GPLv2, the authors of this program hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with the program.
//
// Without limiting the foregoing grant of rights under the GPLv2 and
// additional permission as to separately licensed software, this
// program is also subject to the Universal FOSS Exception, version 1.0,
// a copy of which can be found along with its FAQ at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see 
// http://www.gnu.org/licenses/gpl-2.0.html.

package utility;

import static org.junit.jupiter.api.Assertions.assertEquals;

import com.github.dockerjava.api.DockerClient;
import com.github.dockerjava.api.command.ExecCreateCmdResponse;
import com.github.dockerjava.api.command.InspectContainerResponse;
import com.github.dockerjava.api.exception.DockerException;
import eu.rekawek.toxiproxy.Proxy;
import eu.rekawek.toxiproxy.model.ToxicDirection;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Consumer;
import org.testcontainers.DockerClientFactory;
import org.testcontainers.containers.BindMode;
import org.testcontainers.containers.GenericContainer;
import org.testcontainers.containers.Network;
import org.testcontainers.containers.ToxiproxyContainer;
import org.testcontainers.containers.output.FrameConsumerResultCallback;
import org.testcontainers.containers.output.OutputFrame;
import org.testcontainers.images.builder.ImageFromDockerfile;
import org.testcontainers.utility.DockerImageName;
import org.testcontainers.utility.MountableFile;
import org.testcontainers.utility.TestEnvironment;

public class ContainerHelper {
  private static final String TEST_CONTAINER_IMAGE_NAME = "kitware/cmake:ci-fedora35-x86_64-2022-02-01";
  private static final DockerImageName TOXIPROXY_IMAGE =
      DockerImageName.parse("shopify/toxiproxy:2.1.0");

  public void runExecutable(GenericContainer<?> container, String testDir, String testExecutable)
      throws IOException, InterruptedException {
    System.out.println("==== Container console feed ==== >>>>");
    Consumer<OutputFrame> consumer = new ConsoleConsumer();
    Long exitCode = execInContainer(container, consumer, String.format("./%s/%s", testDir, testExecutable));
    System.out.println("==== Container console feed ==== <<<<");
    assertEquals(0, exitCode, "Some tests failed.");
  }

  public void runCTest(GenericContainer<?> container, String testDir)
      throws IOException, InterruptedException {
    System.out.println("==== Container console feed ==== >>>>");
    Consumer<OutputFrame> consumer = new ConsoleConsumer();
    Long exitCode = execInContainer(container, consumer, "ctest", "--test-dir", testDir, "--output-on-failure");
    System.out.println("==== Container console feed ==== <<<<");
    assertEquals(0, exitCode, "Some tests failed.");
  }

  public GenericContainer<?> createTestContainer(String dockerImageName, String driverPath) {
    return createTestContainer(dockerImageName, driverPath, TEST_CONTAINER_IMAGE_NAME);
  }

  public GenericContainer<?> createTestContainer(
      String dockerImageName,
      String driverPath,
      String testContainerImageName) {
    return new GenericContainer<>(
        new ImageFromDockerfile(dockerImageName, true)
            .withDockerfileFromBuilder(builder ->
                builder
                    .from(testContainerImageName)
                    .run("mkdir", "app")
                    .workDir("/app")
                    .entryPoint("/bin/sh -c \"while true; do sleep 30; done;\"")
                    .build()))
        .withFileSystemBind(driverPath, "/app", BindMode.READ_WRITE)
        .withPrivilegedMode(true) // it's needed to control Linux core settings like TcpKeepAlive
        .withCopyFileToContainer(MountableFile.forHostPath("./gradlew"), "app/gradlew")
        .withCopyFileToContainer(MountableFile.forHostPath("./gradle.properties"), "app/gradle.properties")
        .withCopyFileToContainer(MountableFile.forHostPath("./build.gradle.kts"), "app/build.gradle.kts")
        .withCopyFileToContainer(MountableFile.forHostPath("./src/test/resources/odbc.ini"), "/etc/odbc.ini")
        .withCopyFileToContainer(MountableFile.forHostPath("./src/test/resources/odbcinst.ini"), "/etc/odbcinst.ini");
  }

  protected Long execInContainer(GenericContainer<?> container, Consumer<OutputFrame> consumer,
      String... command)
      throws UnsupportedOperationException, IOException, InterruptedException {
    return execInContainer(container, consumer, StandardCharsets.UTF_8, command);
  }

  protected Long execInContainer(GenericContainer<?> container, Consumer<OutputFrame> consumer,
      Charset outputCharset, String... command)
      throws UnsupportedOperationException, IOException, InterruptedException {
    return execInContainer(container.getContainerInfo(), consumer, outputCharset, command);
  }

  protected Long execInContainer(InspectContainerResponse containerInfo,
      Consumer<OutputFrame> consumer,
      Charset outputCharset, String... command)
      throws UnsupportedOperationException, IOException, InterruptedException {
    if (!TestEnvironment.dockerExecutionDriverSupportsExec()) {
      // at time of writing, this is the expected result in CircleCI.
      throw new UnsupportedOperationException(
          "Your docker daemon is running the \"lxc\" driver, which doesn't support \"docker exec\".");
    }

    if (!isRunning(containerInfo)) {
      throw new IllegalStateException(
          "execInContainer can only be used while the Container is running");
    }

    final String containerId = containerInfo.getId();
    final DockerClient dockerClient = DockerClientFactory.instance().client();

    final ExecCreateCmdResponse execCreateCmdResponse = dockerClient.execCreateCmd(containerId)
        .withAttachStdout(true)
        .withAttachStderr(true)
        .withCmd(command)
        .exec();

    try (final FrameConsumerResultCallback callback = new FrameConsumerResultCallback()) {
      callback.addConsumer(OutputFrame.OutputType.STDOUT, consumer);
      callback.addConsumer(OutputFrame.OutputType.STDERR, consumer);
      dockerClient.execStartCmd(execCreateCmdResponse.getId()).exec(callback).awaitCompletion();
    }

    return dockerClient.inspectExecCmd(execCreateCmdResponse.getId()).exec().getExitCodeLong();
  }

  protected boolean isRunning(InspectContainerResponse containerInfo) {
    try {
      return containerInfo != null && Boolean.TRUE.equals(containerInfo.getState().getRunning());
    } catch (DockerException e) {
      return false;
    }
  }

  public static GenericContainer<?> createMysqlContainer(Network network) {
    return new GenericContainer<>("mysql:8.0.31")
        .withNetwork(network)
        .withNetworkAliases("mysql-instance")
        .withEnv("MYSQL_ROOT_PASSWORD", "root")
        .withEnv("MYSQL_DATABASE", "test")
        .withCommand(
          "--local_infile=1",
          "--secure-file-priv=/var/lib/mysql");
  }

  public ToxiproxyContainer createAndStartProxyContainer(
      final Network network, String networkAlias, String networkUrl, String hostname, int port,
      int expectedProxyPort) {
    final ToxiproxyContainer container = new ToxiproxyContainer(TOXIPROXY_IMAGE)
        .withNetwork(network)
        .withNetworkAliases(networkAlias, networkUrl);
    container.start();
    ToxiproxyContainer.ContainerProxy proxy = container.getProxy(hostname, port);
    assertEquals(expectedProxyPort, proxy.getOriginalProxyPort(),
        "Proxy port for " + hostname + " should be " + expectedProxyPort);
    return container;
  }

  public List<ToxiproxyContainer> createProxyContainers(final Network network,
      List<String> clusterInstances, String proxyDomainNameSuffix) {
    ArrayList<ToxiproxyContainer> containers = new ArrayList<>();
    int instanceCount = 0;
    for (String hostEndpoint : clusterInstances) {
      containers.add(new ToxiproxyContainer(TOXIPROXY_IMAGE)
          .withNetwork(network)
          .withNetworkAliases("toxiproxy-instance-" + (++instanceCount),
              hostEndpoint + proxyDomainNameSuffix));
    }
    return containers;
  }

  // return db cluster instance proxy port
  public int createAuroraInstanceProxies(List<String> clusterInstances,
      List<ToxiproxyContainer> containers, int port) {
    Set<Integer> proxyPorts = new HashSet<>();

    for (int i = 0; i < clusterInstances.size(); i++) {
      String instanceEndpoint = clusterInstances.get(i);
      ToxiproxyContainer container = containers.get(i);
      ToxiproxyContainer.ContainerProxy proxy = container.getProxy(instanceEndpoint, port);
      proxyPorts.add(proxy.getOriginalProxyPort());
    }
    assertEquals(1, proxyPorts.size(), "DB cluster proxies should be on the same port.");
    return proxyPorts.stream().findFirst().orElse(0);
  }
}
