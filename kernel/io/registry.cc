#include <yak/io/registry.hh>

IoRegistry &IoRegistry::getRegistry()
{
	static IoRegistry singleton;
	return singleton;
}
