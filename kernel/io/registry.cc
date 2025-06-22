#include <yak/io/registry.hh>

IO_OBJ_DEFINE_VIRTUAL(Personality, Object);

IoRegistry &IoRegistry::getRegistry()
{
	static IoRegistry singleton;
	return singleton;
}
