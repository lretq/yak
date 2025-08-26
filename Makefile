OUT := build/yak/x86_64/debug/sysroot.tar

.PHONY: $(OUT)
$(OUT):
	@mkdir -p sysroot/
	@make -C sysbuild/
	@cp -v sysbuild/init sysroot/sbin/init
	@cd sysroot && tar cvf ../sysroot.tar sbin/
	mv sysroot.tar $@
