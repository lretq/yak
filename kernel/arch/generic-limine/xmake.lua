-- stylua: ignore start 

rule("limine")
	on_load(function(target)
		target:add("defines", "LIMINE_API_REVISION=3")
		target:add("deps", "yak.boot.limine")
	end)

target("yak.boot.limine")
	set_kind("static")
	add_deps("limine")
	add_defines("LIMINE_API_REVISION=3")
	add_files("entry.c")
