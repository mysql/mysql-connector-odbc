# Contributing Guidelines

We welcome your code contributions. Before submitting code via a GitHub pull
request, or by filing a bug in https://bugs.mysql.com you will need to have
signed the Oracle Contributor Agreement (see https://oca.opensource.oracle.com).

Only pull requests from committers that can be verified as having signed the OCA
can be accepted.

## Reporting Issues

Before reporting a new bug, please check first to see if a similar
bug [exists](https://bugs.mysql.com/search.php).

Bug reports should be as complete as possible.  Please try and include
the following:

* Complete steps to reproduce the issue
* Any information about platform and environment that could be specific to the bug
* Specific version of the product you are using
* Specific version of the server being used
* Code written in C/C++/VB or another language to help reproduce the issue if possible

Please do NOT raise a GitHub Issue to report a security vulnerability.
See SECURITY.md for additional information.

## Submitting Code Contribution

Contributing to this project is easy. You just need to follow these steps.

1. Make sure you have a user account at bugs.mysql.com. You'll need to reference this
    user account when you submit your OCA (Oracle Contributor Agreement).
2. Sign the Oracle OCA. You can find instructions for doing that at the OCA Page,
    at https://oca.opensource.oracle.com
3. Validate your contribution by including tests that sufficiently cover the functionality.
4. Verify that the entire test suite passes with your code applied.
5. Submit your pull request via GitHub or uploading it using the contribution tab to a bug
   record in https://bugs.mysql.com (using the 'contribution' tab).

It is also possible to upload your changes using the 'contribution' tab to
a bug record in https://bugs.mysql.com.

Only pull requests from committers that can be verified as having signed the OCA
can be accepted.

## None-Code Contributions

Submissions Other than Code. These terms apply to all of Your Submissions other than
code contributions. "You" means you personally, as well as any person or entity on
whose behalf you are Using the Site. "You" does not include Oracle or its employees
using the Site on Oracle's behalf. "Use" and its variants are to be interpreted in
their broadest sense and include, without limitation, the acts of using, accessing,
receiving, browsing, downloading from, and uploading to. A "User" is a person or
entity who Uses the site.
"Submissions" means any materials (other than code contributions), including but not
limited to technology specifications, technical materials, documentation, discussion
thread postings, blogs, wikis, data, and any other content, information, technology
or services submitted to by You to the site.
You hereby grant to Oracle and all Users a royalty-free, perpetual, irrevocable,
worldwide, non-exclusive and fully sub-licensable right and license under Your
intellectual property rights to reproduce, modify, adapt, publish, translate, create
derivative works from, distribute, perform, display and use Your Submissions (in whole
or part) and to incorporate or implement them in other works in any form, media, or
technology now known or later developed. This includes, without limitation, the right
to incorporate or implement the Submission into any product or service, and to display,
market, sublicense and distribute the Submissions as incorporated or embedded in any
product or service distributed or offered by Oracle without compensation to you.
All Users, Oracle, and their sublicensees are responsible for any modifications they
make to the Submissions of others.

## Running Tests

Any contributed code should pass our unit tests.
To run the unit tests you need to perform the following steps:

* Build the Connector/ODBC
* Register the new driver with the driver manager and create a DSN
* Run MySQL Server
* Set the following environment variables (optional):
  * TEST_DSN = <the name of DSN previously created> (default = test)
  * TEST_DRIVER = <the name of ODBC driver as registered in odbcinst.ini> (default = MySQL ODBC X.Y Driver, where X.Y is the major/minor version of the driver)
  * TEST_UID = <MySQL user name> (default = root)
  * TEST_PASSWORD = <MySQL password> (default is an empty string)
  * TEST_SOCKET = <the path to the socket file in Unix-like OS> (default is an empty string)
  * TEST_PORT = <the port number> (default = 3306)
* In the OS command line enter the test subdirectory of Connector/ODBC build directory and run `ctest` utility

At the end of `ctest` run the result should indicate 100% tests passed.
