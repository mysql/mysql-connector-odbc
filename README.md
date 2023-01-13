# MySQL Connector/ODBC Driver

You can get the latest stable release from the [MySQL downloads](https://dev.mysql.com/downloads/connector/odbc/).

For detailed information please visit the official [MySQL Connector/ODBC documentation](https://dev.mysql.com/doc/connector-odbc/en/).

Source packages are available from our [github releases page](https://github.com/mysql/mysql-connector-odbc/releases).

**The MySQL Connector/ODBC Driver** allows an application to take advantage of the features of clustered MySQL databases.

## Table of Contents
- [MySQL Connector/ODBC Driver](#mysql-connectorodbc-driver)
  - [Table of Contents](#table-of-contents)
  - [What is Failover?](#what-is-failover)
  - [Benefits of the MySQL Connector/ODBC Driver](#benefits-of-the-mysql-connectorodbc-driver)
  - [Getting Started](#getting-started)
    - [Installing the MySQL Connector/ODBC Driver](#installing-the-mysql-connectorodbc-driver)
      - [Windows](#windows)
      - [MacOS](#macos)
      - [Linux](#linux)
      - [Configuring the Driver and DSN Entries](#configuring-the-driver-and-dsn-entries)
        - [odbc.ini](#odbcini)
        - [odbcinst.ini](#odbcinstini)
  - [Failover Process](#failover-process)
    - [Connection Strings and Configuring the Driver](#connection-strings-and-configuring-the-driver)
    - [Failover Specific Options](#failover-specific-options)
    - [Driver Behaviour During Failover For Different Connection URLs](#driver-behaviour-during-failover-for-different-connection-urls)
    - [Host Pattern](#host-pattern)
  - [Advanced Failover Configuration](#advanced-failover-configuration)
    - [Failover Time Profiles](#failover-time-profiles)
      - [Example of the configuration for a normal failover time profile:](#example-of-the-configuration-for-a-normal-failover-time-profile)
      - [Example of the configuration for an aggressive failover time profile:](#example-of-the-configuration-for-an-aggressive-failover-time-profile)
    - [Writer Cluster Endpoints After Failover](#writer-cluster-endpoints-after-failover)
    - [2-Node Clusters](#2-node-clusters)
    - [Node Availability](#node-availability)
  - [Enhanced Failure Monitoring](#enhanced-failure-monitoring)
    - [The Benefits Enhanced Failure Monitoring](#the-benefits-enhanced-failure-monitoring)
      - [Simple Network Timeout](#simple-network-timeout)
    - [Enabling Host Monitoring](#enabling-host-monitoring)
    - [Enhanced Failure Monitoring Parameters](#enhanced-failure-monitoring-parameters)
    - [Failover Exception Codes](#failover-exception-codes)
      - [08S01 - Communication Link Failure](#08s01---communication-link-failure)
      - [08S02 - Communication Link Changed](#08s02---communication-link-changed)
        - [Sample Code](#sample-code)
      - [08007 - Connection Failure During Transaction](#08007---connection-failure-during-transaction)
        - [Sample Code](#sample-code-1)
  - [Building the MySQL Connector/ODBC Driver](#building-the-mysql-connectorodbc-driver)
    - [Windows](#windows-1)
    - [MacOS](#macos-1)
      - [Troubleshoot](#troubleshoot)
    - [Linux](#linux-1)
  - [Testing the MySQL Connector/ODBC Driver](#testing-the-mysql-connectorodbc-driver)
    - [Unit Tests](#unit-tests)
    - [Integration Tests](#integration-tests)
      - [Integration Tests Against A MySQL Server](#integration-tests-against-a-mysql-server)
        - [Prerequisites](#prerequisites)
        - [Steps](#steps)
      - [Failover-specific Integration Tests](#failover-specific-integration-tests)
        - [Prerequisites](#prerequisites-1)
        - [Steps](#steps-1)
  - [Getting Help and Opening Issues](#getting-help-and-opening-issues)
    - [Logging](#logging)
      - [Enabling Logs On Windows](#enabling-logs-on-windows)
        - [Example](#example)
      - [Enabling Logs On MacOS and Linux](#enabling-logs-on-macos-and-linux)
  - [License](#license)

## What is Failover?

In an Amazon Aurora database (DB) cluster, failover is a mechanism by which Aurora automatically repairs the DB cluster status when a primary DB instance becomes unavailable. It achieves this goal by electing an Aurora Replica to become the new primary DB instance, so that the DB cluster can provide maximum availability to a primary read-write DB instance. The MySQL Connector/ODBC Driver is designed to coordinate with this behavior in order to provide minimal downtime in the event of a DB instance failure.

## Benefits of the MySQL Connector/ODBC Driver

Although Aurora is able to provide maximum availability through the use of failover, existing client drivers do not currently support this functionality. This is partially due to the time required for the DNS of the new primary DB instance to be fully resolved in order to properly direct the connection. The MySQL Connector/ODBC Driver fully utilizes failover behavior by maintaining a cache of the Aurora cluster topology and each DB instance's role (Aurora Replica or primary DB instance). This topology is provided via a direct query to the Aurora database, essentially providing a shortcut to bypass the delays caused by DNS resolution. With this knowledge, the MySQL Connector/ODBC Driver can more closely monitor the Aurora DB cluster status so that a connection to the new primary DB instance can be established as fast as possible. Additionally, the MySQL Connector/ODBC Driver can be used to interact with Aurora MySQL, RDS MySQL, and commercial/open-source MySQL databases.

## Getting Started

### Installing the MySQL Connector/ODBC Driver

#### Windows

Download the `.msi` Windows installer for your system; execute the installer and follow the onscreen instructions. The default target installation location for the driver files is `C:\Program Files\MySQL\Connector ODBC 8.0`. An ANSI driver and a Unicode driver will be installed, named respectively `MySQL ODBC ANSI Driver` and `MySQL ODBC 8.0 Unicode Driver`. To uninstall the ODBC driver, open the same installer file, select the option to uninstall the driver and follow the onscreen instructions to successfully uninstall the driver.

#### MacOS

In order to use the MySQL Connector/ODBC Driver, [iODBC Driver Manager](http://www.iodbc.org/dataspace/doc/iodbc/wiki/iodbcWiki/Downloads) must be installed. `iODBC Driver Manager` contains the required libraries to install, configure the driver and DSN configurations.

Download the `.pkg` installer; run the installer and follow the onscreen instructions. The default target installation location for the driver folder is `/usr/local/`. Note that for a MacOS system, additional steps are required to configure the driver and Data Source Name (DSN) entries before you can use the driver(s). Initially, the installer will register two driver entries with two corresponding DSN entries. For information about [how to configure the driver and DSN settings](#configuring-the-driver-and-dsn-entries), review the configuration sample. There is no uninstaller at this time, but all the driver files can be removed by deleting the target installation directory.

#### Linux

In order to use the MySQL Connector/ODBC Driver, [unixODBC](http://www.unixodbc.org/) must be installed.

For **Ubuntu 64 bit**:

```bash
sudo apt update
sudo apt install unixodbc
```

For **Amazon Linux 2 64 bit**:

```bash
sudo yum update
sudo yum install unixODBC
```

Once `unixODBC` is installed, download the `.tar.gz` file, and extract the contents to your desired location. For a Linux system, additional steps are required to configure the driver and Data Source Name (DSN) entries before the driver(s) can be used. For more information, see [Configuring the Driver and DSN settings](#configuring-the-driver-and-dsn-entries). There is no uninst

#### Configuring the Driver and DSN Entries 

To configure the driver on Windows, use the `ODBC Data Source Administrator` tool to add or configure a DSN for either the `MySQL ODBC ANSI Driver` or `MySQL ODBC 8.0 Unicode Driver`. With this DSN you can specify the options for the desired connection. Additional configuration properties are available by clicking the `Details >>` button.

To use the driver on MacOS or Linux systems, you need to create two files (`odbc.ini` and `odbcinst.ini`), that will contain the configuration for the driver and the Data Source Name (DSN).

You can modify the files manually, or through tools with a GUI such as `iODBC Administrator` (available for MacOS). In the following sections, we show samples of `odbc.ini` and `odbcinst.ini` files that describe how an ANSI driver could be set up for a MacOS system. In a MacOS system, the `odbc.ini` and `odbcinst.ini` files are typically located in the `/Library/ODBC/` directory.

For a Linux system, the files would be similar, but the driver file would have the `.so` extension instead of the `.dylib` extension shown in the sample. On a Linux system, the `odbc.ini` and `odbcinst.ini` files are typically located in the `/etc` directory.

##### odbc.ini

```bash
[ODBC Data Sources]
myodbcw = ODBC Unicode Driver for MySQL
myodbca = ODBC ANSI Driver for MySQL

[myodbcw]
Driver                             = ODBC Unicode Driver for MySQL
SERVER                             = localhost
NO_SCHEMA                          = 1
TOPOLOGY_REFRESH_RATE              = 30000
FAILOVER_TIMEOUT                   = 60000
FAILOVER_TOPOLOGY_REFRESH_RATE     = 5000
FAILOVER_WRITER_RECONNECT_INTERVAL = 5000
FAILOVER_READER_CONNECT_TIMEOUT    = 30000
CONNECT_TIMEOUT                    = 30
NETWORK_TIMEOUT                    = 30

[myodbca]
Driver                             = ODBC ANSI Driver for MySQL
SERVER                             = localhost
NO_SCHEMA                          = 1
TOPOLOGY_RR                        = 30000
FAILOVER_TIMEOUT                   = 60000
FAILOVER_TOPOLOGY_REFRESH_RATE     = 5000
FAILOVER_WRITER_RECONNECT_INTERVAL = 5000
FAILOVER_READER_CONNECT_TIMEOUT    = 30000
CONNECT_TIMEOUT                    = 30
NETWORK_TIMEOUT                    = 30
```

##### odbcinst.ini

```bash
[ODBC Drivers]
ODBC Unicode Driver for MySQL = Installed
ODBC ANSI Driver for MySQL    = Installed

[ODBC Unicode Driver for MySQL]
Driver = /usr/local/MySQL_ODBC_Connector/lib/myodbc8w.dylib

[ODBC ANSI Driver for MySQL]
Driver = /usr/local/MySQL_ODBC_Connector/lib/myodbc8a.dylib
```

## Failover Process

In an Amazon Aurora database (DB) cluster, failover is a mechanism by which Aurora automatically repairs the DB cluster status when a primary DB instance becomes unavailable. It achieves this goal by electing an Aurora Replica to become the new primary DB instance, so that the DB cluster can provide maximum availability to a primary read-write DB instance. The MySQL Connector/ODBC Driver uses the Failover Plugin to coordinate with this behavior in order to provide minimal downtime in the event of a DB instance failure.

![failover_diagram](docs/images/failover_diagram.png)

The figure above provides a simplified overview of how the MySQL Connector/ODBC Driver handles an Aurora failover encounter. Starting at the top of the diagram, an application using the driver sends a request to get a logical connection to an Aurora database.

In this example, the application requests a connection using the Aurora DB cluster endpoint and is returned a logical connection that is physically connected to the primary DB instance in the DB cluster, DB instance C. By design, details about which specific DB instance the physical connection is connected to have been abstracted away.

Over the course of the application's lifetime, it executes various statements against the logical connection. If DB instance C is stable and active, these statements succeed and the application continues as normal. If DB instance C experiences a failure, Aurora will initiate failover to promote a new primary DB instance. At the same time, the MySQL Connector/ODBC Driver will intercept the related communication exception and kick off its own internal failover process.

If the primary DB instance has failed, the driver will use its internal topology cache to temporarily connect to an active Aurora Replica. This Aurora Replica will be periodically queried for the DB cluster topology until the new primary DB instance is identified (DB instance A or B in this case).

At this point, the driver will connect to the new primary DB instance and return control to the application to allow the user to reconfigure the session state as needed. Although the DNS endpoint for the DB cluster might not yet resolve to the new primary DB instance, the driver has already discovered this new DB instance during its failover process, and will be directly connected to it when the application continues executing statements. In this way the driver provides a faster way to reconnect to a newly promoted DB instance, thus increasing the availability of the DB cluster.

### Connection Strings and Configuring the Driver

To set up a connection, the driver uses an ODBC connection string. An ODBC connection string specifies a set of semicolon-delimited connection options. Typically, a connection string will either:

1. specify a Data Source Name containing a preconfigured set of options (`DSN=xxx;`) or
2. configure options explicitly (`SERVER=xxx;PORT=xxx;...;`). This option will override values set inside the DSN.

### Failover Specific Options

In addition to the parameters that you can configure for the [MySQL Connector/ODBC driver](https://dev.mysql.com/doc/connector-odbc/en/connector-odbc-configuration-connection-parameters.html), you can configure the following parameters in a DSN or connection string to specify failover behaviour. If the values for these options are not specified, the default values will be used. If you are dealing with the Windows DSN UI, click `Details >>` and navigate to the `Cluster Failover` tab to find the equivalent parameters.

| Option                             | Description                                                                                                                                                                                                                                                                                                                                                                        | Type   | Required | Default                                  |
| ---------------------------------- |------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|----------|------------------------------------------|
| `ENABLE_CLUSTER_FAILOVER`            | Set to `1` to enable the fast failover behaviour offered by the MySQL Connector/ODBC Driver.                                                                                                                                                                                                                                                                                         | bool   | No       | `1`                                      |
| `ALLOW_READER_CONNECTIONS`           | Set to `1` to allow connections to reader instances during the failover process.                                                                                                                                                                                                                                                                                                   | bool   | No       | `0`                                      |
| `GATHER_PERF_METRICS`                | Set to `1` to record failover-associated metrics.                                                                                                                                                                                                                                                                                                                                  | bool   | No       | `0`                                      |
| `GATHER_PERF_METRICS_PER_INSTANCE`   | Set to `1` to gather additional performance metrics per instance as well as cluster.                                                                                                                                                                                                                                                                                               | bool   | No       | `0`                                      |
| `HOST_PATTERN`                       | This parameter is not required unless connecting to an AWS RDS cluster via an IP address or custom domain URL. In those cases, this parameter specifies the cluster instance DNS pattern that will be used to build a complete instance endpoint. A "?" character in this pattern should be used as a placeholder for the DB instance identifiers of the instances in the cluster.  <br/><br/>Example: `?.my-domain.com`, `any-subdomain.?.my-domain.com:9999`<br/><br/>Usecase Example: If your cluster instance endpoint follows this pattern:`instanceIdentifier1.customHost`, `instanceIdentifier2.customHost`, etc. and you want your initial connection to be to `customHost:1234`, then your connection string should look like this: `SERVER=customHost;PORT=1234;DATABASE=test;HOST_PATTERN=?.customHost` <br><br/> If the provided connection string is not an IP address or custom domain, the driver will automatically acquire the cluster instance host pattern from the customer-provided connection string. | char\* | If connecting using an IP address or custom domain URL: Yes <br><br> Otherwise: No <br><br> See [Host Pattern](#host-pattern) for more details. | `NONE`                                   |
| `CLUSTER_ID`                         | A unique identifier for the cluster. Connections with the same cluster ID share a cluster topology cache. This connection parameter is not required and thus should only be set if desired.                                                                                                                                                                                        | char\* | No       | Either the cluster ID or the instance ID, depending on whether the provided connection string is a cluster or instance URL. |
| `TOPOLOGY_REFRESH_RATE`              | Cluster topology refresh rate in milliseconds. The cached topology for the cluster will be invalidated after the specified time, after which it will be updated during the next interaction with the connection.                                                                                                                                                                   | int    | No       | `30000`                                  |
| `FAILOVER_TIMEOUT`                   | Maximum allowed time in milliseconds to attempt reconnecting to a new writer or reader instance after a cluster failover is initiated.                                                                                                                                                                                                                                             | int    | No       | `60000`                                  |
| `FAILOVER_TOPOLOGY_REFRESH_RATE`     | Cluster topology refresh rate in milliseconds during a writer failover process. During the writer failover process, cluster topology may be refreshed at a faster pace than normal to speed up discovery of the newly promoted writer.                                                                                                                                             | int    | No       | `5000`                                   |
| `FAILOVER_WRITER_RECONNECT_INTERVAL` | Interval of time in milliseconds to wait between attempts to reconnect to a failed writer during a writer failover process.                                                                                                                                                                                                                                                        | int    | No       | `5000`                                   |
| `FAILOVER_READER_CONNECT_TIMEOUT`    | Maximum allowed time in milliseconds to attempt a connection to a reader instance during a reader failover process.                                                                                                                                                                                                                                                                | int    | No       | `30000`                                  |
| `CONNECT_TIMEOUT`                    | Timeout (in seconds) for socket connect, with 0 being no timeout.                                                                                                                                                                                                                                                                                                                  | int    | No       | `30`                                     |
| `NETWORK_TIMEOUT`                    | Timeout (in seconds) on network socket operations, with 0 being no timeout.                                                                                                                                                                                                                                                                                                        | int    | No       | `30`                                     |

### Driver Behaviour During Failover For Different Connection URLs

![failover_behaviour](docs/images/failover_behaviour.jpg)

### Host Pattern

When connecting to Aurora clusters, this parameter is required when the connection string does not provide enough information about the database cluster domain name. If the Aurora cluster endpoint is used directly, the driver will recognize the standard Aurora domain name and can re-build a proper Aurora instance name when needed. In cases where the connection string uses an IP address, a custom domain name or localhost, the driver won't know how to build a proper domain name for a database instance endpoint. For example, if a custom domain was being used and the cluster instance endpoints followed a pattern of `instanceIdentifier1.customHost`, `instanceIdentifier2.customHost`, etc, the driver would need to know how to construct the instance endpoints using the specified custom domain. Because there isn't enough information from the custom domain alone to create the instance endpoints, the `HOST_PATTERN` should be set to `?.customHost`, making the connection string `SERVER=customHost;PORT=1234;DATABASE=test;HOST_PATTERN=?.customHost`. Refer to [Driver Behaviour During Failover For Different Connection URLs](#driver-behaviour-during-failover-for-different-connection-urls) for more examples.

## Advanced Failover Configuration

### Failover Time Profiles

A failover time profile refers to a specific combination of failover parameters that determine the time in which failover should be completed and define the aggressiveness of failover. Some failover parameters include `FAILOTVER_TIMEOUT` and `FAILOVER_READER_CONNECT_TIMEOUT`. Failover should be completed within 5 minutes by default. If the connection is not re-established during this time, then the failover process times out and fails. Users can configure the failover parameters to adjust the aggressiveness of the failover and fulfill the needs of their specific application. For example, a user could take a more aggressive approach and shorten the time limit on failover to promote a fail-fast approach for an application that does not tolerate database outages. Examples of normal and aggressive failover time profiles are shown below. 
<br><br>
**:warning:Note**: Aggressive failover does come with its side effects. Since the time limit on failover is shorter, it becomes more likely that a problem is caused not by a failure, but rather because of a timeout.
<br><br>

#### Example of the configuration for a normal failover time profile:

| Parameter                              | Value     |
|----------------------------------------|-----------|
| `FAILOTVER_TIMEOUT`                    | `180000 ` |
| `FAILOVER_WRITER_RECONNECT_INTERVAL`   | `2000`    |
| `FAILOVER_READER_CONNECT_TIMEOUT`      | `30000`   |
| `FAILOVER_TOPOLOGY_REFRESH_RATE`       | `2000`    |

#### Example of the configuration for an aggressive failover time profile:

| Parameter                              | Value   |
|----------------------------------------|---------|
| `FAILOTVER_TIMEOUT`                    | `30000` |
| `FAILOVER_WRITER_RECONNECT_INTERVAL`   | `2000`  |
| `FAILOVER_READER_CONNECT_TIMEOUT`      | `10000` |
| `FAILOVER_TOPOLOGY_REFRESH_RATE`       | `2000`  |

### Writer Cluster Endpoints After Failover

Connecting to a writer cluster endpoint after failover can result in a faulty connection because DNS causes a delay in changing the writer cluster. On the AWS DNS server, this change is updated usually between 15-20 seconds, but the other DNS servers sitting between the application and the AWS DNS server may not be updated in time. Using this stale DNS data will most likely cause problems for users, so it is important to keep this is mind.

### 2-Node Clusters

Using failover with a 2-node cluster is not beneficial because during the failover process involving one writer node and one reader node, the two nodes simply switch roles; the reader becomes the writer and the writer becomes the reader. If failover is triggered because one of the nodes has a problem, this problem will persist because there aren't any extra nodes to take the responsibility of the one that is broken. Three or more database nodes are recommended to improve the stability of the cluster.

### Node Availability

It seems as though just one node, the one triggering the failover, will be unavailable during the failover process; this is actually not true. When failover is triggered, all nodes become unavailable for a short time. This is because the control plane, which orchestrates the failover process, first shuts down all nodes, then starts the writer node, and finally starts and connects the remaining nodes to the writer. In short, failover requires each node to be reconfigured and thus, all nodes must become unavailable for a short period of time. One additional note to point out is that if your failover time is aggressive, then this may cause failover to fail because some nodes may still be unavailable by the time your failover times out.

## Enhanced Failure Monitoring

<div style="text-align:center"><img src="docs/images/enhanced_failure_monitoring_diagram.png"/></div>

The figure above shows a simplified workflow of Enhanced Failure Monitoring (EFM). Enhanced Failure Monitoring is a feature that is available within the MySQL Connector/ODBC Driver. There is a monitor that will periodically check the connected database node's health, or availability. If a database node is determined to be unhealthy, the connection will be aborted. This check uses the [Enhanced Failure Monitoring Parameters](#enhanced-failure-monitoring-parameters) and a database node's responsiveness to determine whether a node is healthy.

### The Benefits Enhanced Failure Monitoring

Enhanced Failure Monitoring helps user applications detect failures earlier. When a user application executes a query, EFM may detect that the connected database node is unavailable. When this happens, the query is cancelled and the connection will be aborted. This allows queries to fail fast instead of waiting indefinitely or failing due to a timeout.

One use case is to pair EFM with [Failover](#failover-specific-options). When EFM discovers a database node failure, the connection will be aborted. Without Failover enabled, the connection would be terminated up to the user application level. If Failover is enabled, the MySQL Connector/ODBC Driver can attempt to failover to a different, healthy database node where the query can be executed.

Not all user applications will have a need for Enhanced Failure Monitoring. If a user application's query times are predictable and short, and the application does not execute any long-running SQL queries, Enhanced Failure Monitoring may be replaced with a simple network timeout that would consume fewer resources and would be simpler to configure. 

Although this is a viable alternative, EFM is more configurable than simple network timeouts. Users should keep these advantages and disadvantages in mind when deciding whether Enhanced Failure Monitoring is suitable for their application.

#### Simple Network Timeout

This option is useful when a user application executes quick statements that run for predictable lengths of time. In this case, the network timeout should be set to a value such as the 95th to 99th percentile. One can set network timeout by setting the `NETWORK_TIMEOUT` property in the `odbc.ini` file, or appending `NETWORK_TIMEOUT=?;` to the connection string (where `?` indicates the timeout value in seconds) or in the UI to configure the DSN.

### Enabling Host Monitoring

Enhanced Failure Monitoring is enabled by default in MySQL Connector/ODBC Driver, but it can be disabled with the parameter `ENABLE_FAILURE_DETECTION` is set to `0` in `odbc.ini` or in the connection string.
> 
### Enhanced Failure Monitoring Parameters

<div style="text-align:center"><img src="docs/images/efm_monitor_process.png" /></div>
The parameters `FAILURE_DETECTION_TIME`, `FAILURE_DETECTION_INTERVAL`, and `FAILURE_DETECTION_COUNT` are similar to TCP Keepalive parameters. Each connection has its own set of parameters. The `FAILURE_DETECTION_TIME` is how long the monitor waits after a SQL query is started to send a probe to a database node. The `FAILURE_DETECTION_INTERVAL` is how often the monitor sends a probe to a database node. The `FAILURE_DETECTION_COUNT` is how many times a monitor probe can go unacknowledged before the database node is deemed unhealthy. 
The parameter `FAILURE_DETECTION_TIMEOUT` is how long the monitor waits for the probe finishes, and every connection to the same server endpoint will have the same timeout, which is set during the first connection to that server.

To determine the health of a database node: 
1. The monitor will first wait for a time equivalent to the `FAILURE_DETECTION_TIME`. 
2. Then, every `FAILURE_DETECTION_INTERVAL`, the monitor will send a probe to the database node. 
3. The monitor waits for the probe coming back up to `FAILURE_DETECTION_TIMEOUT` seconds.
4. If the probe is not acknowledged by the database node OR the monitor has timed out, a counter is incremented. 
5. If the counter reaches the `FAILURE_DETECTION_COUNT`, the database node will be deemed unhealthy and the connection will be aborted.

If a more aggressive approach to failure checking is necessary, all of these parameters can be reduced to reflect that. However, increased failure checking may also lead to an increase in false positives. For example, if the `FAILURE_DETECTION_INTERVAL` was shortened, the plugin may complete several connection checks that all fail. The database node would then be considered unhealthy, but it may have been about to recover and the connection checks were completed before that could happen.

To configure failure detection, you can specify the following parameters in a DSN or connection string. If the values for these options are not specified, the default values will be used. If you are dealing with the Windows DSN UI, click `Details >>` and navigate to the `Monitoring` tab to find the equivalent parameters.

| Option                             | Description                                                                                                                                                                                                                                                                                                                                                                        | Type   | Required | Default                                  |
| ---------------------------------- |------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------|----------|------------------------------------------|
| `ENABLE_FAILURE_DETECTION`         | Set to `1` to enable the fast failover behaviour offered by the MySQL Connector/ODBC Driver for MySQL.                                                                                                                                                                                                                                                                                         | bool   | No       | `1`                                      |
| `FAILURE_DETECTION_TIME`           | Interval in milliseconds between sending a SQL query to the server and the first probe to the database node.                                                                                                                                                                                                                                                                       | int    | No       | `30000`                                  |
| `FAILURE_DETECTION_INTERVAL`       | Interval in milliseconds between probes to database node.                                                                                                                                                                                                                                                                                                                          | int    | No       | `5000`                                   |
| `FAILURE_DETECTION_COUNT`          | Number of failed connection checks before considering database node as unhealthy.                                                                                                                                                                                                                                                                                                  | int    | No       | `3`                                      |
| `FAILURE_DETECTION_TIMEOUT`        | Amount of time the monitor waits for the probe before timing out.            | int | No | `5` |
| `MONITOR_DISPOSAL_TIME`            | Interval in milliseconds for a monitor to be considered inactive and to be disposed.                                                                                                                                                                                                                                                                                               | int    | No       | `60000`                                  |

> :heavy_exclamation_mark: **Always ensure you provide a non-zero network timeout value or a connect timeout value in your DSN**
>
> MySQL Connector/ODBC Driver has a default non-zero value for `NETWORK_TIMEOUT` and `CONNECT_TIMEOUT`. If one decides to alter those values and set those values to 0, EFM may wait forever to establish a monitoring connection in the event where the database node is unavailable. As a general rule, **do not** override those values to 0.
>### :warning: Warnings About Usage of the MySQL Connector/ODBC Driver with RDS Proxy
> It is recommended to either disable Host Monitoring, or to avoid using RDS Proxy endpoints when the Host Monitoring is enabled.
>
> Although using RDS Proxy endpoints with the Enhanced Failure Monitoring doesn't cause any critical issues, this approach is not recommended. The main reason is that RDS Proxy transparently re-routes requests to one database instance. RDS Proxy decides which database instance is used based on many criteria, and it's on a per-request basis. Such switching between different instances makes Host Monitoring useless in terms of instance health monitoring. It will not be able to identify what actual instance it is connected to and which one it's monitoring. That could be a source of false positive failure detections. At the same time, it could still proactively monitor network connectivity to RDS Proxy endpoints and report outages back to a user application if they occur.

### Failover Exception Codes

#### 08S01 - Communication Link Failure

When the driver returns an error code ```08S01```, the original connection failed, and the driver tried to failover to a new instance, but was not able to. There are various reasons this may happen: no nodes were available, a network failure occurred, and so on. In this scenario, please wait until the server is up or other problems are solved.

#### 08S02 - Communication Link Changed

When the driver returns an error code ```08S02```, the original connection failed while autocommit was set to true, and the driver successfully failed over to another available instance in the cluster. However, any session state configuration of the initial connection is now lost. In this scenario, you should:

1. Reconfigure and reuse the original connection (the reconfigured session state will be the same as the original connection).
2. Repeat the query that was executed when the connection failed and continue work as desired.

##### Sample Code

```cpp
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

// Scenario 1: Failover happens when autocommit is set to true - SQLRETURN with code 08S02.

void setInitialSessionState(SQLHDBC dbc, SQLHSTMT handle) {
    const auto set_timezone = (SQLCHAR*)"SET time_zone = \"+00:00\"";
    SQLExecDirect(handle, set_timezone, SQL_NTS);
}

bool executeQuery(SQLHDBC dbc, SQLCHAR* query) {
    int MAX_RETRIES = 5;
    SQLHSTMT handle;
    SQLCHAR sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
    SQLRETURN ret;

    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle);

    int retries = 0;
    bool success = false;

    while (true) {
        ret = SQLExecDirect(handle, query, SQL_NTS);

        if (SQL_SUCCEEDED(ret)) {
            success = true;
            break;
        }
        else {
            if (retries > MAX_RETRIES) {
                break;
            }

            // Check what kind of error has occurred
            SQLSMALLINT stmt_length;
            SQLINTEGER native_error;
            SQLError(nullptr, nullptr, handle, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
            const std::string state = (char*)sqlstate;

            // Failover has occurred and the driver has failed over to another instance successfully
            if (state.compare("08S02") == 0) {
                // Reconfigure the connection
                setInitialSessionState(dbc, handle);
                // Re-execute that query again
                retries++;
            }
            else {
                // If other exceptions occur
                break;
            }
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, handle);
    return success;
}

int main() {
    SQLHENV env;
    SQLHDBC dbc;
    SQLSMALLINT len;
    SQLRETURN ret;
    SQLCHAR conn_in[4096], conn_out[4096];

    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    const char* dsn = "ODBCDriverDSN";
    const char* user = "username";
    const char* pwd = "password";
    const char* server = "db-identifier-cluster-XYZ.us-east-2.rds.amazonaws.com";
    int port = 3306;
    const char* db = "employees";

    sprintf(reinterpret_cast<char*>(conn_in), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;PORT=%d;DATABASE=%s;", dsn, user, pwd, server, port, db);
    
    // attempt a connection
    ret = SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    
    if (SQL_SUCCEEDED(ret)) {
        // if the connection is successful, execute a query using the MySQL Connector/ODBC Driver
        const auto sleep_stmt = (SQLCHAR*)"SELECT * from employees*";
        executeQuery(dbc, sleep_stmt);
    }

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}
```

#### 08007 - Connection Failure During Transaction

When the driver returns an error code ```08007```, the original connection failed within a transaction (while autocommit was set to false). In this scenario, when the transaction ends, the driver first attempts to rollback the transaction, and then fails over to another available instance in the cluster. Note that the rollback might be unsuccessful as the initial connection may be broken at the time that the driver recognizes the problem. Note also that any session state configuration of the initial connection is now lost. In this scenario, you should:

1. Reconfigure and reuse the original connection (the reconfigured session state will be the same as the original connection).
2. Re-start the transaction and repeat all queries which were executed during the transaction before the connection failed.
3. Repeat the query that was executed when the connection failed, and continue work.

##### Sample Code

```cpp
#include <iostream>
#include <sql.h>
#include <sqlext.h>

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

// Scenario 2: Failover happens when autocommit is set to false - SQLRETURN with code 08007.

void setInitialSessionState(SQLHDBC dbc, SQLHSTMT handle) {
    const auto set_timezone = (SQLCHAR*)"SET time_zone = \"+00:00\"";
    const auto setup_autocommit_query = (SQLCHAR*)"SET autocommit = 0";

    SQLExecDirect(handle, set_timezone, SQL_NTS);
    SQLExecDirect(handle, setup_autocommit_query, SQL_NTS);
}

bool executeQuery(SQLHDBC dbc) {
    int MAX_RETRIES = 5;
    SQLHSTMT handle;
    SQLCHAR sqlstate[6], message[SQL_MAX_MESSAGE_LENGTH];
    SQLRETURN ret;

    // Queries to execute in a transaction
    SQLCHAR* queries[] = {
        (SQLCHAR*)"INSERT INTO employees(emp_no, birth_date, first_name, last_name, gender, hire_date) VALUES (5000000, '1958-05-01', 'John', 'Doe', 'M', '1997-11-30')",
        (SQLCHAR*)"INSERT INTO employees(emp_no, birth_date, first_name, last_name, gender, hire_date) VALUES (5000001, '1958-05-01', 'Mary', 'Malcolm', 'F', '1997-11-30')",
        (SQLCHAR*)"INSERT INTO employees(emp_no, birth_date, first_name, last_name, gender, hire_date) VALUES (5000002, '1958-05-01', 'Tom', 'Jerry', 'M', '1997-11-30')"
    };

    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle);
    setInitialSessionState(dbc, handle);

    int retries = 0;
    bool success = false;

    while (true) {
        for (SQLCHAR* query : queries) {
            ret = SQLExecDirect(handle, query, SQL_NTS);
        }
        SQLEndTran(SQL_HANDLE_DBC, dbc, SQL_COMMIT);

        if (SQL_SUCCEEDED(ret)) {
            success = true;
            break;
        } else {
            if (retries > MAX_RETRIES) {
                break;
            }

            // Check what kind of error has occurred
            SQLSMALLINT stmt_length;
            SQLINTEGER native_error;
            SQLError(nullptr, nullptr, handle, sqlstate, &native_error, message, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length);
            const std::string state = (char*)sqlstate;

            // Failover has occurred and the driver has failed over to another instance successfully
            if (state.compare("08007") == 0) {
                // Reconfigure the connection
                setInitialSessionState(dbc, handle);
                // Re-execute every queriy that was inside the transaction
                retries++;
            }
            else {
                // If other exceptions occur
                break;
            }
        }
    }

    return success;
}

int main() {
    SQLHENV env;
    SQLHDBC dbc;
    SQLSMALLINT len;
    SQLRETURN rc;
    SQLCHAR conn_in[4096], conn_out[4096];

    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    const char* dsn = "ODBCDriverDSN";
    const char* user = "username";
    const char* pwd = "password";
    const char* server = "db-identifier-cluster-XYZ.us-east-2.rds.amazonaws.com";
    int port = 3306;
    const char* db = "employees";

    sprintf(reinterpret_cast<char*>(conn_in), "DSN=%s;UID=%s;PWD=%s;SERVER=%s;PORT=%d;DATABASE=%s;", dsn, user, pwd, server, port, db);
    
    // attempt a connection
    rc = SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT);
    
    if (SQL_SUCCEEDED(rc)) {
        // if the connection is successful, execute a query using the MySQL Connector/ODBC Driver
        executeQuery(dbc);
    }

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}
```

>### :warning: Warnings About Proper Usage of the MySQL Connector/ODBC Driver
>It is highly recommended that you use the cluster and read-only cluster endpoints instead of the direct instance endpoints of your Aurora cluster, unless you are confident about your application's use of instance endpoints. Although the driver will correctly failover to the new writer instance when using instance endpoints, use of these endpoints is discouraged because individual instances can spontaneously change reader/writer status when failover occurs. The driver will always connect directly to the instance specified if an instance endpoint is provided, so a write-safe connection cannot be assumed if the application uses instance endpoints.

## Building the MySQL Connector/ODBC Driver

### Windows

<!-- TODO: Verify that the driver can be built with newer versions of Visual Studio after rebasing -->
1. Install the following programs to build the driver:
    - [CMake](https://cmake.org/download/)
    - [Visual Studio](https://visualstudio.microsoft.com/downloads/)
      > The driver has been built successfully using `Visual Studio 2019`, and it may not build correctly with other versions. When installing Visual Studio, ensure the `Visual C++ 2019` and `Visual Studio Tools for CMake` packages are also installed.
    - [MySQL Server](https://dev.mysql.com/downloads/installer/)
2. Build the driver in the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Visual Studio 16 2019" -DMYSQL_DIR="C:\Program Files\MySQL\MySQL Server 8.0" -DMYSQLCLIENT_STATIC_LINKING=TRUE
    cmake --build build --config Release
    ```

### MacOS

1. Install the following packages available on `Homebrew` or other package management system of your choice:
     - `libiodbc`
     - `cmake`
     - `mysql-client`
     - `mysql`
2. Set the environment variable MYSQL_DIR as the path to your `mysql-client` installation location:
    ```
    export MYSQL_DIR=/usr/local/opt/mysql-client
    ```
3. Build the driver in the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DODBC_INCLUDES=/usr/local/Cellar/libiodbc/3.52.15/include
    cmake --build build --config Release
    ```
    Note: you may have a different `libiodbc` version. Change `3.52.15` to your respective version.

#### Troubleshoot

If you encounter an `ld: library not found for -lzstd` error, run the following command, and then rebuild the driver:
```
export LIBRARY_PATH=$LIBRARY_PATH:$(brew --prefix zstd)/lib/
```

If you encounter an `ld: library not found for -lssl` error, run one of the following commands (depending on what openssl library you have) and then rebuild the driver:
```
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/opt/openssl/lib/
```
or
```
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/opt/openssl@1.1/lib/
```

### Linux

1. Install the following required packages:
    ```
    sudo apt-get update
    sudo apt-get install build-essential libgtk-3-dev libmysqlclient-dev unixodbc unixodbc-dev
    ```
2. Build the driver in the `build` directory with the following commands:
    ```
    cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DWITH_UNIXODBC=1
    cmake --build build --config Release
    ```

## Testing the MySQL Connector/ODBC Driver

### Unit Tests

1. Build driver binaries with `ENABLE_UNIT_TESTS` command set to `TRUE`:
   - **Windows**
        ```
        cmake -S . -B build -G "Visual Studio 16 2019" -DMYSQL_DIR="C:\Program Files\MySQL\MySQL Server 8.0" -DMYSQLCLIENT_STATIC_LINKING=TRUE -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
   - **MacOS**
        ```
        cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DODBC_INCLUDES=/usr/local/Cellar/libiodbc/3.52.15/include -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
   - **Linux**
        ```
        cmake -S . -B build -G "Unix Makefiles" -DMYSQLCLIENT_STATIC_LINKING=true -DWITH_UNIXODBC=1 -DENABLE_UNIT_TESTS=TRUE
        cmake --build build --config Release
        ```
2. There are two options to run the unit tests:
    - Run `ctest` directly from the `unit_testing` directory.
    - Navigate to `unit_testing/bin/Release` and run `unit_testing.exe`. To specify a particular test or test suite, include `--gtest_filter` in the command.

The following example demonstrates running all the tests in the `TopologyServiceTest` suite with the `.\unit_testing.exe --gtest_filter=TopologyServiceTest.*` command:

```
PS C:\Other\dev\mysql-connector-odbc\unit_testing\bin\Release> .\unit_testing.exe --gtest_filter=TopologyServiceTest.*
Running main() from C:\Other\dev\mysql-connector-odbc\_deps\googletest-src\googletest\src\gtest_main.cc
Note: Google Test filter = TopologyServiceTest.*
[==========] Running 7 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 7 tests from TopologyServiceTest
[ RUN      ] TopologyServiceTest.TopologyQuery
[       OK ] TopologyServiceTest.TopologyQuery (0 ms)
[ RUN      ] TopologyServiceTest.MultiWriter
[       OK ] TopologyServiceTest.MultiWriter (0 ms)
[ RUN      ] TopologyServiceTest.CachedTopology
[       OK ] TopologyServiceTest.CachedTopology (0 ms)
[ RUN      ] TopologyServiceTest.QueryFailure
[       OK ] TopologyServiceTest.QueryFailure (0 ms)
[ RUN      ] TopologyServiceTest.StaleTopology
[       OK ] TopologyServiceTest.StaleTopology (1007 ms)
[ RUN      ] TopologyServiceTest.RefreshTopology
[       OK ] TopologyServiceTest.RefreshTopology (1013 ms)
[ RUN      ] TopologyServiceTest.ClearCache
[       OK ] TopologyServiceTest.ClearCache (0 ms)
[----------] 7 tests from TopologyServiceTest (2026 ms total)

[----------] Global test environment tear-down
[==========] 7 tests from 1 test suite ran. (2030 ms total)
[  PASSED  ] 7 tests.
```

### Integration Tests

There are two types of integration tests you can run. One type is an integration test against a MySQL Server, and the other type consists of the two sets of integration tests specific to the failover functionality provided by the MySQL Connector/ODBC Driver.

#### Integration Tests Against A MySQL Server

##### Prerequisites

- Install MySQL Server. See the [build instructions for the desired system](#building-the-mysql-connector/odbc-driver) for instructions.
- [**Optional**] Install [Docker](https://docs.docker.com/get-docker/).

##### Steps

1. Specify the following environment variables on your target platform before building the driver:
    | Environment Variable | Description                                                     | Example                         | Platforms               |
    |----------------------|-----------------------------------------------------------------|---------------------------------|-------------------------|
    | TEST_DSN             | The DSN to use for the test                                     | ODBCDriverDSN                   | All systems             |
    | TEST_USERNAME        | The name of the user with access to the MySQL Server            | root                            | All systems             |
    | TEST_PASSWORD        | The password for the test database user                         | root                            | All systems             |
    | TEST_DATABASE        | The test database                                               | test                            | All systems             |
    | DYLD_LIBRARY_PATH    | The path to the library folder of your MySQL server directory   | /usr/local/opt/mysql-client/lib | MacOS systems           |
    | ODBCINI              | The path to your odbc.ini file                                  | /etc/odbc.ini                   | MacOS and Linux systems |
    | ODBCINSTINI          | The path to your odbcinst.ini file                              | /etc/odbcinst.ini               | MacOS and Linux systems |
    > **NOTE:** The `TEST_PASSWORD` environment variable is only required if you have specified a password for the `root` user when installing the MySQL Server.
2. Build and install the driver for a specific platform as described in [Installing the MySQL Connector/ODBC Driver](#installing-the-mysql-connector/odbc-driver).
3. Start the MySQL Server. You may either start a local server or use a docker images. 
4. [**Optional**] To start the MySQL Server via a Docker image. Navigate to `test/docker` and execute `docker-compose up -d` to start the server in the background.
5. Navigate to the `test` directory and execute `ctest`.

#### Failover-specific Integration Tests

> **NOTE:** This set of tests can only be run on Linux at the moment.

##### Prerequisites

- Install JDK 8:
  ```
  sudo apt-get install openjdk-8-jdk
  ```
- Install [Docker](https://docs.docker.com/get-docker/)
  
##### Steps

> **NOTE:** Running these tests will automatically create an Amazon Aurora MySQL DB cluster with at least 5 instances and may induce a cost. Ensure the test cluster is cleaned up after testing on the [Amazon RDS Management Console](https://console.aws.amazon.com/rds/home).

1. This set of tests runs against an Amazon Aurora MySQL DB cluster with at least 5 instances. The test will automatically generate the required AWS MySQL DB cluster and instances if proper AWS credentials are set up. Refer to the [documentation](https://docs.aws.amazon.com/AmazonRDS/latest/AuroraUserGuide/CHAP_SettingUp_Aurora.html) for information about setting up a development environment for Amazon Aurora.
2. Define the following environment variables:
    | Environment Variable       | Description                                                                                                         | Example                                    |
    |----------------------------|---------------------------------------------------------------------------------------------------------------------|--------------------------------------------|
    | TEST_DSN                   | The DSN to use for the test.                                                                                        | ODBCDriverDSN                              |
    | TEST_USERNAME              | The name of the user with access to the Amazon Aurora MySQL DB cluster.                                             | username                                   |
    | TEST_PASSWORD              | The password for the test database user.                                                                            | password                                   |
    | TEST_DB_CLUSTER_IDENTIFIER | The unique identifier for the Amazon Aurora MySQL DB cluster.                                                       | db-identifier                              |
    | AWS_ACCESS_KEY_ID          | The access key ID to your AWS account.                                                                              | `ASIAIOSFODNN7EXAMPLE`                     |
    | AWS_SECRET_ACCESS_KEY      | The secret access key for your AWS account.                                                                         | `wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY` |
    | AWS_SESSION_TOKEN          | The AWS session token for your AWS count. This is only required if you have set up temporary security credentials.  | `AQoDYXdzEJr...`                           |
    | DRIVER_PATH                | The directory where the driver was built to.                                                                        | `~/dev/mysql-connector-odbc/build`               |
3. Ensure the following packages are installed on Linux:
    ```
    sudo apt-get update && sudo apt-get install \
                build-essential \
                libgtk-3-dev \
                libmysqlclient-dev \
                unixodbc \
                unixodbc-dev \
                curl  \
                libcurl4-openssl-dev
    ```
4. Build the driver with the following commands (the following commands assume the driver source code is stored within `~/dev/mysql-connector-odbc`):
   ```
    cmake -E make_directory ~/dev/mysql-connector-odbc/build
    cmake -S . -B build \
        -G "Unix Makefiles" \
        -DCMAKE_BUILD_TYPE=Release \
        -DMYSQLCLIENT_STATIC_LINKING=TRUE \
        -DENABLE_INTEGRATION_TESTS=TRUE \
        -DWITH_UNIXODBC=1
    cmake --build build --config Release
   ```
5. Navigate to the `testframework` directory and run the command: `./gradlew --no-parallel --no-daemon test-failover --info`.
6. Log files are written to the `build` directory as `myodbc.log`.

## Getting Help and Opening Issues

If you encounter a bug with the MySQL Connector/ODBC Driver, we would like to hear about it. Please search the [existing issues](https://github.com/mysql/mysql-connector-odbc/issues) and see if others are also experiencing the issue before opening a new issue. When opening a new issue, we will need the version of MySQL Connector/ODBC Driver, C++ language version, OS you’re using, and the MySQL database version you're running against. Please include a reproduction case for the issue when appropriate.

The GitHub issues are intended for bug reports and feature requests. Keeping the list of open issues lean will help us respond in a timely manner.

### Logging

If you encounter an issue with the MySQL Connector/ODBC Driver and would like to report it, please include driver logs if possible, as they help us diagnose problems quicker.

#### Enabling Logs On Windows

When connecting the MySQL Connector/ODBC Driver using a Windows system, ensure logging is enabled by following the steps below:

1. Open the ODBC Data Source Administrator.
2. Add a new DSN or configure an existing DSN of your choice.
3. Open the details for the DSN.
4. Navigate to the Debug tab.
5. Ensure the box to log queries is checked.

##### Example

![enable-logging-windows](docs/images/enable_logging_windows.jpg)

The resulting log file, named `myodbc.log`, can be found under `%temp%`.

#### Enabling Logs On MacOS and Linux

When connecting the MySQL Connector/ODBC Driver using a MacOS or Linux system, include the `LOG_QUERY` parameter in the connection string with the value of `1` to enable logging (`DSN=XXX;LOG_QUERY=1;...`). A log file, named `myodbc.log`, will be produced. On MacOS, the log file can be located in `/tmp`. On Linux, the log file can be found in the current working directory.

## License

This software is released under version 2 of the GNU General Public License (GPLv2).