rm -r ./build
cmake -S . -B ./build 
cd build
make all
./myncdu ../test
cd ..