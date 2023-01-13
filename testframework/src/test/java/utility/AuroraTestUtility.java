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

import com.amazonaws.auth.AWSCredentialsProvider;
import com.amazonaws.auth.DefaultAWSCredentialsProviderChain;
import com.amazonaws.services.ec2.AmazonEC2;
import com.amazonaws.services.ec2.AmazonEC2ClientBuilder;
import com.amazonaws.services.ec2.model.AmazonEC2Exception;
import com.amazonaws.services.ec2.model.AuthorizeSecurityGroupIngressRequest;
import com.amazonaws.services.ec2.model.RevokeSecurityGroupIngressRequest;
import com.amazonaws.services.rds.AmazonRDS;
import com.amazonaws.services.rds.AmazonRDSClientBuilder;
import com.amazonaws.services.rds.model.CreateDBClusterRequest;
import com.amazonaws.services.rds.model.CreateDBInstanceRequest;
import com.amazonaws.services.rds.model.DeleteDBClusterRequest;
import com.amazonaws.services.rds.model.DeleteDBInstanceRequest;
import com.amazonaws.services.rds.model.DescribeDBInstancesRequest;
import com.amazonaws.services.rds.model.DescribeDBInstancesResult;
import com.amazonaws.services.rds.model.Filter;
import com.amazonaws.services.rds.model.Tag;
import com.amazonaws.services.rds.waiters.AmazonRDSWaiters;
import com.amazonaws.util.StringUtils;
import com.amazonaws.waiters.NoOpWaiterHandler;
import com.amazonaws.waiters.Waiter;
import com.amazonaws.waiters.WaiterParameters;
import com.amazonaws.waiters.WaiterTimedOutException;
import com.amazonaws.waiters.WaiterUnrecoverableException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.URL;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;

/**
 * Creates and destroys AWS RDS Cluster and Instances AWS Credentials is loaded using
 * DefaultAWSCredentialsProviderChain Environment Variable > System Properties > Web Identity Token
 * > Profile Credentials > EC2 Container To specify which to credential provider, use
 * AuroraTestUtility(String region, AWSCredentialsProvider credentials) *
 * <p>
 * If using environment variables for credential provider Required - AWS_ACCESS_KEY -
 * AWS_SECRET_KEY
 */
public class AuroraTestUtility {

  // Default values
  private String dbUsername = "my_test_username";
  private String dbPassword = "my_test_password";
  private String dbName = "test";
  private String dbIdentifier = "test-identifier";
  private String dbEngine = "aurora-mysql";
  private String dbInstanceClass = "db.r5.large";
  private final String dbRegion;
  private final String dbSecGroup = "default";
  private int numOfInstances = 5;

  private AmazonRDS rdsClient = null;
  private AmazonEC2 ec2Client = null;
  private String runnerIP = "";

  private static final String DUPLICATE_IP_ERROR_CODE = "InvalidPermission.Duplicate";

  /**
   * Initializes an AmazonRDS & AmazonEC2 client. RDS client used to create/destroy clusters &
   * instances. EC2 client used to add/remove IP from security group.
   */
  public AuroraTestUtility() {
    this("us-east-1");
  }

  /**
   * Initializes an AmazonRDS & AmazonEC2 client.
   *
   * @param region define AWS Regions, refer to https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/Concepts.RegionsAndAvailabilityZones.html
   */
  public AuroraTestUtility(String region) {
    this(region, new DefaultAWSCredentialsProviderChain());
  }

  /**
   * Initializes an AmazonRDS & AmazonEC2 client.
   *
   * @param region      define AWS Regions, refer to https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/Concepts.RegionsAndAvailabilityZones.html
   * @param credentials Specific AWS credential provider
   */
  public AuroraTestUtility(String region, AWSCredentialsProvider credentials) {
    dbRegion = region;

    rdsClient = AmazonRDSClientBuilder
        .standard()
        .withRegion(dbRegion)
        .withCredentials(credentials)
        .build();

    ec2Client = AmazonEC2ClientBuilder
        .standard()
        .withRegion(dbRegion)
        .withCredentials(credentials)
        .build();
  }

  /**
   * Creates RDS Cluster/Instances and waits until they are up, and proper IP whitelisting for
   * databases.
   *
   * @param username   Master username for access to database
   * @param password   Master password for access to database
   * @param identifier Database identifier
   * @param name       Database name
   * @return An endpoint for one of the instances
   * @throws InterruptedException when clusters have not started after 30 minutes
   */
  public AuroraClusterInfo createCluster(String username, String password, String identifier, String name)
      throws InterruptedException {
    dbUsername = username;
    dbPassword = password;
    dbIdentifier = identifier;
    dbName = name;
    return createCluster();
  }

  /**
   * Creates RDS Cluster/Instances and waits until they are up, and proper IP whitelisting for
   * databases.
   *
   * @return An endpoint for one of the instances
   * @throws InterruptedException when clusters have not started after 30 minutes
   */
  public AuroraClusterInfo createCluster() throws InterruptedException {
    final List<String> instances = new ArrayList<>();
    final Tag testRunnerTag = new Tag()
    .withKey("env")
    .withValue("test-runner");
    
    // Create Cluster
    final CreateDBClusterRequest dbClusterRequest = new CreateDBClusterRequest()
        .withDBClusterIdentifier(dbIdentifier)
        .withDatabaseName(dbName)
        .withMasterUsername(dbUsername)
        .withMasterUserPassword(dbPassword)
        .withSourceRegion(dbRegion)
        .withEnableIAMDatabaseAuthentication(true)
        .withEngine(dbEngine)
        .withStorageEncrypted(true)
        .withTags(testRunnerTag);

    rdsClient.createDBCluster(dbClusterRequest);

    // Create Instances
    final CreateDBInstanceRequest dbInstanceRequest = new CreateDBInstanceRequest()
        .withDBClusterIdentifier(dbIdentifier)
        .withDBInstanceClass(dbInstanceClass)
        .withEngine(dbEngine)
        .withPubliclyAccessible(true)
        .withTags(testRunnerTag);

    for (int i = 1; i <= numOfInstances; i++) {
      final String instanceId = dbIdentifier + "-" + i;
      rdsClient.createDBInstance(
          dbInstanceRequest.withDBInstanceIdentifier(dbIdentifier + "-" + i));
      instances.add(instanceId);
    }

    // Wait for all instances to be up
    final AmazonRDSWaiters waiter = new AmazonRDSWaiters(rdsClient);
    final DescribeDBInstancesRequest dbInstancesRequest = new DescribeDBInstancesRequest()
        .withFilters(new Filter().withName("db-cluster-id").withValues(dbIdentifier));
    final Waiter<DescribeDBInstancesRequest> instancesRequestWaiter = waiter
        .dBInstanceAvailable();
    final Future<Void> future = instancesRequestWaiter.runAsync(
        new WaiterParameters<>(dbInstancesRequest), new NoOpWaiterHandler());
    try {
      future.get(30, TimeUnit.MINUTES);
    } catch (WaiterUnrecoverableException | WaiterTimedOutException | TimeoutException | ExecutionException exception) {
      deleteCluster();
      throw new InterruptedException(
          "Unable to start AWS RDS Cluster & Instances after waiting for 30 minutes");
    }

    final DescribeDBInstancesResult dbInstancesResult = rdsClient.describeDBInstances(
        dbInstancesRequest);
    final String endpoint = dbInstancesResult.getDBInstances().get(0).getEndpoint().getAddress();
    final String suffix = endpoint.substring(endpoint.indexOf('.') + 1);

    return new AuroraClusterInfo(
        suffix,
        dbIdentifier + ".cluster-" + suffix,
        dbIdentifier + ".cluster-ro-" + suffix,
        instances.stream().map(item -> item + "." + suffix).collect(Collectors.toList())
    );
  }

  /**
   * Gets public IP
   *
   * @return public IP of user
   * @throws UnknownHostException
   */
  public String getPublicIPAddress() throws UnknownHostException {
    String ip;
    try {
      URL ipChecker = new URL("http://checkip.amazonaws.com");
      BufferedReader reader = new BufferedReader(new InputStreamReader(
          ipChecker.openStream()));
      ip = reader.readLine();
    } catch (Exception e) {
      throw new UnknownHostException("Unable to get IP");
    }
    return ip;
  }

  /**
   * Authorizes IP to EC2 Security groups for RDS access.
   */
  public void ec2AuthorizeIP(String ipAddress) {
    if (StringUtils.isNullOrEmpty(ipAddress)) {
      return;
    }
    final AuthorizeSecurityGroupIngressRequest authRequest = new AuthorizeSecurityGroupIngressRequest()
        .withGroupName(dbSecGroup)
        .withCidrIp(ipAddress + "/32")
        .withIpProtocol("TCP") // All protocols
        .withFromPort(3306) // For all ports
        .withToPort(3306);

    try {
      ec2Client.authorizeSecurityGroupIngress(authRequest);
    } catch (AmazonEC2Exception exception) {
      if (!DUPLICATE_IP_ERROR_CODE.equalsIgnoreCase(exception.getErrorCode())) {
        throw exception;
      }
    }
  }

  /**
   * De-authorizes IP from EC2 Security groups.
   *
   * @throws UnknownHostException
   */
  public void ec2DeauthorizesIP(String ipAddress) {
    if (StringUtils.isNullOrEmpty(ipAddress)) {
      return;
    }
    final RevokeSecurityGroupIngressRequest revokeRequest = new RevokeSecurityGroupIngressRequest()
        .withGroupName(dbSecGroup)
        .withCidrIp(ipAddress + "/32")
        .withIpProtocol("TCP") // All protocols
        .withFromPort(3306) // For all ports
        .withToPort(3306);

    try {
      ec2Client.revokeSecurityGroupIngress(revokeRequest);
    } catch (AmazonEC2Exception exception) {
      // Ignore
    }
  }

  /**
   * Destroys all instances and clusters. Removes IP from EC2 whitelist.
   *
   * @param identifier database identifier to delete
   */
  public void deleteCluster(String identifier) {
    dbIdentifier = identifier;
    deleteCluster();
  }

  /**
   * Destroys all instances and clusters. Removes IP from EC2 whitelist.
   */
  public void deleteCluster() {
    // Tear down instances
    final DeleteDBInstanceRequest dbDeleteInstanceRequest = new DeleteDBInstanceRequest()
        .withSkipFinalSnapshot(true);

    for (int i = 1; i <= numOfInstances; i++) {
      rdsClient.deleteDBInstance(
          dbDeleteInstanceRequest.withDBInstanceIdentifier(dbIdentifier + "-" + i));
    }

    // Tear down cluster
    final DeleteDBClusterRequest dbDeleteClusterRequest = new DeleteDBClusterRequest()
        .withSkipFinalSnapshot(true)
        .withDBClusterIdentifier(dbIdentifier);

    rdsClient.deleteDBCluster(dbDeleteClusterRequest);
  }
}
