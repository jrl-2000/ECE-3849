# invoke SourceDir generated makefile for rtos.pem4f
rtos.pem4f: .libraries,rtos.pem4f
.libraries,rtos.pem4f: package/cfg/rtos_pem4f.xdl
	$(MAKE) -f E:\ECE\ECE3849\ece3849_lab2_starter/src/makefile.libs

clean::
	$(MAKE) -f E:\ECE\ECE3849\ece3849_lab2_starter/src/makefile.libs clean

