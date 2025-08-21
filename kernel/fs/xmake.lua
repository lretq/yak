-- stylua: ignore start

target("yak.fs")
	set_kind("static")
	add_rules("kernel")
	add_files("**.c")

target("yak.fs.builtin")
	set_kind("phony")
	add_deps("yak.fs")
