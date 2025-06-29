#include <yak/io/device.hh>
#include <nanoprintf.h>

IO_OBJ_DEFINE_VIRTUAL(Driver, TreeNode);
IO_OBJ_DEFINE_VIRTUAL(Device, TreeNode);

void Driver::init()
{
	TreeNode::init();
}

void Device::init()
{
	TreeNode::init();
}
