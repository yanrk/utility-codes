# process
autoscan
mv configure.scan configure.ac
autoheader
aclocal
autoconf
automake --add-missing
./configure
make
make dist
make distcheck
make distclean



# sample
su
yum -y install automake autoconf
yum -y install libtool
yum -y install m4
exit

cd source-codes
autoheader
aclocal
autoconf
libtoolize --force
automake --add-missing
./configure
make
make install
