set DEST_ROOT=\\192.168.0.4\maverick\8126\arm-linux-2.6.28\udp_svr

mkdir %DEST_ROOT%
copy Makefile %DEST_ROOT%\ /D
copy 8126.config %DEST_ROOT%\ /D
copy *.h %DEST_ROOT%\ /D
copy *.c %DEST_ROOT%\ /D
copy *.a %DEST_ROOT%\ /D

mkdir %DEST_ROOT%\include
copy .\include\*.h %DEST_ROOT%\include\ /D

