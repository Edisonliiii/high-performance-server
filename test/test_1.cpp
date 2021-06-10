
#include <stdio.h>
#include <vector>
#include <iostream>

enum MEM_CAP
{
  u4K   = 4096,
  u16K  = 16384,
  u64K  = 65536,
  u256K = 262144,
  u1M   = 1048576,
  u4M   = 4194304,
  u8M   = 8388608
}E_t;

int main(int argc, char const *argv[])
{
	std::vector<int> v = {1,2,3,4};
	std::vector<int>* ptr = &v;
	std::cout<<(*ptr)[3]<<std::endl;
    //printf("%p\n", &E_t::u4K);
	return 0;
}