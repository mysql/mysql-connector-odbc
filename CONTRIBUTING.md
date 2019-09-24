# Contributing Guidelines

We love getting feedback from our users. Bugs and code contributions are great forms of feedback and we thank you for any bugs you report or code you contribute.

## Reporting Issues

Before reporting a new bug, please check first to see if a similar bug [exists](https://bugs.mysql.com/search.php).

Bug reports should be as complete as possible.  Please try and include the following:

* Complete steps to reproduce the issue
* Any information about platform and environment that could be specific to the bug
* Specific version of the product you are using
* Specific version of the server being used
* Code written in C/C++/VB or another language to help reproduce the issue if possible

## Contributing Code

Contributing to this project is easy. You just need to follow these steps.

* Sign the Oracle Contributor Agreement. You can find instructions for doing that at [OCA Page](https://www.oracle.com/technetwork/community/oca-486395.html)
* Develop your pull request
  * Make sure you are aware of the requirements for the project (i.e. don't require C++17 if we are supporting C++11 and higher)
* Validate your pull request by including tests that sufficiently cover the functionality
* Verify that the entire test suite passes with your code applied
* Submit your pull request

## Running Tests

Any contributed code should pass our unit tests.
To run the unit tests you need to perform the following steps:

* Build the Connector/ODBC
* Register the new driver with the driver manager and create a DSN
* Run MySQL Server
* Set the following environment variables (optional):
  * TEST_DSN = <the name of DSN previously created> (default = test)
  * TEST_DRIVER = <the name of ODBC driver as registered in odbcinst.ini> (default = MySQL ODBC 8.0 Driver)
  * TEST_UID = <MySQL user name> (default = root)
  * TEST_PASSWORD = <MySQL password> (default is an empty string)
  * TEST_SOCKET = <the path to the socket file in Unix-like OS> (default is an empty string)
  * TEST_PORT = <the port number> (default = 3306)
* In the OS command line enter the test subdirectory of Connector/ODBC build directory and run `ctest` utility

At the end of `ctest` run the result should indicate 100% tests passed.
