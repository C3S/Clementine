echo 'our fancy new shell script for jenkins!'
cd git/bin
ls
cmake -DWITH_DEBIAN=ON ../
ls
make 
make test
make deb

