plugins {
    java
}

group = "software.aws.rds"
version = "1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

dependencies {
    testImplementation("com.amazonaws:aws-java-sdk-rds:1.12.150")
    testImplementation("com.amazonaws:aws-java-sdk-ec2:1.12.154")
    testImplementation("org.junit.jupiter:junit-jupiter-api:5.8.2")
    testImplementation("org.testcontainers:toxiproxy:1.16.3")
    testImplementation("org.testcontainers:mysql:1.16.3")
    testRuntimeOnly("org.junit.jupiter:junit-jupiter-engine")
}

tasks.getByName<Test>("test") {
    useJUnitPlatform()
}

tasks.register<Test>("test-failover") {
    filter.includeTestsMatching("host.IntegrationContainerTest.testRunFailoverTestInContainer")
}

tasks.register<Test>("test-community") {
    filter.includeTestsMatching("host.IntegrationContainerTest.testRunCommunityTestInContainer")
}

tasks.withType<Test> {
    useJUnitPlatform()
    group = "verification"
    this.testLogging {
        this.showStandardStreams = true
    }
}
