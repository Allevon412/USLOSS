#!/bin/bash

make clean
rm ./testcase_output/test_output_*
echo "[*] post cleaning activities [cntrl] + C to quit, [enter] to continue"
read
echo ""
echo "[*] continuing, building all test cases & building phase1 library"
make
for i in {0..36}
do
	if [ $i -lt 10 ]
	then
		make test0$i
	else
		make test$i
	fi
done
echo ""
echo "[*] built all binaries, [cntrl] + C to quit, [enter] to coninue"
read
echo "[*] continuing, executing all programs and directing stderr + stdout to files"
for i in {0..36}
do
	if [ $i -lt 10 ]
	then
		echo "[*] Performing Test 0$i - Please review output in the following file: ./testcase_output/test_output_0$i.txt"
		./test0$i 2>&1 > ./testcase_output/test_output_0$i.txt 
	else
		echo "[*] Performing Test $i - Please review output in the following file: ./testcase_output/test_output_$i.txt"
		./test$i 2>&1 > ./testcase_output/test_output_$i.txt
	fi
done
echo ""
echo "[*] All executables have been executed. [cntrl] + C to quit, or [enter] to continue"
read
for i in {0..36}
do
        if [ $i -lt 10 ]
        then
		echo "[*] printing output for test 0$i"
		cat ./testcase_output/test_output_0$i.txt
		echo "[*] [cntrl] + C to quit, press enter to continue reading output."
		read
        else
		echo "[*] printing output for test $i"
		cat ./testcase_output/test_output_$i.txt
		echo "[*] [cntrl] + C to quit, press enter to continue reading output."
		read
        fi
done
