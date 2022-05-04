Joshua Dahl - CS457

#Build and Run Instructions
The project has been developed using cmake and git submodules. All of the files required to build the project should be in the distributed zip folder (with the notable exception of the boost libraries which can be easily installed via the package manager and are already installed on the ECC machines).
To actually build, navigate to the root directory of the project (the one containing thirdparty and src) and run:

```bash
mkdir build
cd build
cmake .. # If this step fails try again with: CXX=g++ cmake ..
make
```

Once the project has been built it can be run using:

```bash
./pa4 # In the newly created build directory
```
There are currently no command line options. Simply start entering SQL in the provided
prompt.
The “.exit” command can be used to close the application.

A demo (demo.mp4) is included, it shows the program running with operations multiplexed between the two processes. During the entire demo the folder representing the database is open in the top left where all of the changes being made can be observed.

**NOTE: My DBMS follows the capitalization rules of standard SQL, thus identifiers (tables, column, etc…) are case sensitive while other keywords are not.**