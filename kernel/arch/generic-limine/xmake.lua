-- stylua: ignore start 

rule("limine")
	on_load(function(target)
		target:add("defines", "LIMINE_API_REVISION=3")
		target:add("files", "$(projectdir)/kernel/arch/generic-limine/**.c")
		target:add("deps", "limine")
	end)
