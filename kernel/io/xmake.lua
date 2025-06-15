-- stylua: ignore start

target("yak.io")
	set_kind("static")
	add_rules("kernel")
	add_files("**.c")
	add_files("**.cc")

target("yak.io.builtin")
	set_kind("phony")
	add_deps("yak.io")
