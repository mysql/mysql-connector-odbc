Copyright (c) 1995, 2022, Oracle and/or its affiliates.

This is a release of MySQL Connector/ODBC (formerly MyODBC), the driver that
enables ODBC applications to communicate with MySQL servers.

License information can be found in the LICENSE file.
This distribution may include materials developed by third parties. For license
and attribution notices for these materials, please refer to the LICENSE file.

ABOUT THE DRIVER
================

MySQL Connector/ODBC is brought to you by the MySQL team at Oracle.
The MySQL Connector/ODBC Driver allows an application to take advantage of the features of 
clustered MySQL databases.

-- What is Failover?
An Amazon Aurora DB cluster uses failover to automatically repair the DB cluster 
status when a primary DB instance becomes unavailable. During failover, Aurora 
promotes a replica to become the new primary DB instance, so that the DB cluster 
can provide maximum availability to a primary read-write DB instance. The MySQL 
ODBC Driver is designed to coordinate with this behavior in order to 
provide minimal downtime in the event of a DB instance failure.

-- Benefits of the MySQL Connector/ODBC Driver
Although Aurora is able to provide maximum availability through the use of failover, 
existing client drivers do not fully support this functionality. This is partially 
due to the time required for the DNS of the new primary DB instance to be fully 
resolved in order to properly direct the connection. The MySQL ODBC Driver 
fully utilizes failover behavior by maintaining a cache of the Aurora cluster 
topology and each DB instance's role (Aurora Replica or primary DB instance). This 
topology is provided via a direct query to the Aurora database, essentially providing 
a shortcut to bypass the delays caused by DNS resolution. With this knowledge, the 
MySQL Connector/ODBC Driver can more closely monitor the Aurora DB cluster status so that a 
connection to the new primary DB instance can be established as fast as possible. 
Additionally, the MySQL ODBC Driver can be used to interact with RDS and 
MySQL databases as well as Aurora MySQL.

DOCUMENTATION LOCATION
======================

You can find the documentation on the MySQL website at
<https://dev.mysql.com/doc/connector-odbc/en/>

For the new features/bugfix history, see release notes at
<https://dev.mysql.com/doc/relnotes/connector-odbc/en/news-8-0.html>.
Note that the initial releases used major version 2.0.

For more documentation, visit <https://github.com/mysql/mysql-connector-odbc>. 
This page contain details on how the driver works and how to use it.

CONTACT
=======

For general discussion of the MySQL Connector/ODBC please use the ODBC
community forum at <http://forums.mysql.com/list.php?37> or join
the MySQL Connector/ODBC mailing list at <http://lists.mysql.com>.

Bugs can be reported at <http://bugs.mysql.com/report.php>. Please
use the "Connector / ODBC" or "Connector / ODBC Documentation" bug
category.

LICENSE
=======
This software is released under version 2 of the GNU General Public License (GPLv2).
