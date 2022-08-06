#include "foobar2000.h"

void search_filter_v2::test_multi_here(metadb_handle_list& ref, abort_callback& abort) {
	pfc::array_t<bool> mask; mask.resize(ref.get_size()); 
	this->test_multi_ex(ref, mask.get_ptr(), abort);
	ref.filter_mask(mask.get_ptr());
}
