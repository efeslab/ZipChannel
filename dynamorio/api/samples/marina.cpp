# include "dr_api.h"
#include "drmgr.h"
#include "drx.h"
#include "drreg.h"
#include <stdlib.h> /* qsort */
#include <unordered_map>
#include <map>
#include "drsyms.h"
#include <ctype.h>
#include <vector>
#include <string>
#include <cstring>
#include <set>

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
//#define COLOR_BLUE    "\x1b[34m"
#define COLOR_BLUE    "\x1b[38;5;69m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#define SIZEOF(X) (sizeof(X)/sizeof(X[0]))

bool reg_is_tainted(reg_id_t reg); // function declaration. Didn't bother to put all here
class taint_reg_data_t;
class taint_mem_data_t;
extern std::unordered_map<reg_t, taint_reg_data_t> g_tainted_regs;
extern std::unordered_map<void*, taint_mem_data_t> g_tainted_memory;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Taint tracking /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

typedef unsigned long tag_t;
class tags_t {
	std::set<unsigned long> tags;

public:
	tags_t() : tags() {}
	tags_t(tag_t tag) : tags() {
		tags.insert(tag);
	}

	tags_t(const tags_t& t1, const tags_t& t2) : tags() {
		tags.insert(t1.tags.begin(), t1.tags.end());
		tags.insert(t2.tags.begin(), t2.tags.end());
	}
	using iterator = std::set<unsigned long>::iterator;
	iterator begin() { return tags.begin(); }
	iterator end() { return tags.end(); }
	iterator find(const int& x) { return tags.find(x); }

	void print() {
		dr_fprintf(STDERR, "tags_t: ");
		for (auto t : tags) {
			dr_fprintf(STDERR, "%ld ", t);
		}
		dr_fprintf(STDERR, "\n");
	}
};

class taint_mem_data_t {
	std::map<int, tags_t> bit2tags;

public:
	using iterator = std::map<int, tags_t>::iterator;
	iterator begin() { return bit2tags.begin(); }
	iterator end() { return bit2tags.end(); }
	iterator find(const int& x) { return bit2tags.find(x); }
	tags_t& operator[](int i) { return bit2tags[i]; }
};

class taint_reg_data_t {
	std::map<int, tags_t> bit2tags;

public:
	using iterator = std::map<int, tags_t>::iterator;
	iterator begin() { return bit2tags.begin(); }
	iterator end() { return bit2tags.end(); }
	iterator find(const int& x) { return bit2tags.find(x); }
	tags_t& operator[](int i) { return bit2tags[i]; }
	bool empty() { return bit2tags.empty(); }
	void erase (iterator position) { bit2tags.erase(position); }

	void print(int tabs = 0) {
		/*
		for(auto b2t : this->bit2tags) {
			dr_fprintf(STDERR, "taint_reg_data_t: bit = %ld ", b2t.first);
			b2t.second.print();
		}
		*/
		if(this->bit2tags.empty()) { // this check seems to prevent a crash
			return;
		}
		std::set<tag_t> tags;
		for (auto b2t : bit2tags) {
			tags.insert(b2t.second.begin(), b2t.second.end());
		}
		int max_bit = (bit2tags.rbegin())->first;
		for (auto t : tags) {
			for(int i = 0 ; i < tabs ; i ++) {
				dr_fprintf(STDERR, "\t");
			}
			dr_fprintf(STDERR, "%8ld: ", t);
			for(int i = max_bit ; i >= 0 ; i --) {
				dr_fprintf(STDERR, "|");
				if(bit2tags[i].find(t) != bit2tags[i].end()) {
					dr_fprintf(STDERR, " x");
				} else {
					dr_fprintf(STDERR, "  ");
				}
			}
			dr_fprintf(STDERR, "|\n");
		}
		for(int i = 0 ; i < tabs ; i ++) {
			dr_fprintf(STDERR, "\t");
		}
		for(unsigned int i = 0 ; i < 8 + strlen(": ") ; i ++) {
			dr_fprintf(STDERR, " ");
		}
		for(int i = max_bit ; i >= 0 ; i --) {
			dr_fprintf(STDERR, "|%2d", i);
		}
		dr_fprintf(STDERR, "|\n");
	}

	taint_reg_data_t() {}
	taint_reg_data_t(taint_reg_data_t& t1, taint_reg_data_t& t2) {
		bit2tags = t1.bit2tags;
		for (auto p : t2) {
			int bit = p.first;
			tags_t tags = p.second;
			if(bit2tags.find(p.first) == bit2tags.end()) {
				bit2tags[bit] = tags;
			} else {
				bit2tags[bit] = tags_t(bit2tags[bit], tags);
			}
		}
	}
	taint_reg_data_t(void* addr, size_t size) { // c'tor from memory address
		for (size_t i = 0 ; i < size ; i ++) {
			auto taint_mem_byte = g_tainted_memory.find((char*)addr + i);
			if(taint_mem_byte == g_tainted_memory.end()) {
				continue;
			}

			taint_mem_data_t taint_byte_data = taint_mem_byte->second;
			for(int b = 0 ; b < 8 ; b ++) { // b is the bit in byte
				auto tag = taint_byte_data.find(b);
				if(tag == taint_byte_data.end()) {
					continue;
				}
				bit2tags[b + i * 8] = tag->second;
			}
		}
	}

	static taint_reg_data_t shift_left_ctor(taint_reg_data_t in, int shift_amount, int nr_bits)
	{
		taint_reg_data_t res;
		
		for(auto b2t : in) {
			if(b2t.first + shift_amount < nr_bits) {
				res[b2t.first + shift_amount] = b2t.second;
			}
		}

		return res;
	}

	static taint_reg_data_t shift_right_ctor(taint_reg_data_t in, int shift_amount)
	{
		taint_reg_data_t res;
		
		for(auto b2t : in) {
			if(b2t.first - shift_amount >= 0) {
				res[b2t.first - shift_amount] = b2t.second;
			}
		}

		return res;
	}
}; // this is for gp registers. not AVX XXX: I think this comment is irrelevant now

#define MAX_INSTRUCTION (15)
#define STR_MAX_LEN (256)
typedef struct { /* taint_history_entry_t */
	bool from_read = false; // mark the taint source
	bool chain_end = false; // this is for a memory access that is dependent on taint
	std::vector<char>	src_data;
	unsigned long		src_addr = -1;
	unsigned long		dst_addr = -1;

	typedef struct {
		const char* printable_name;
		char value[64];
		size_t reg_size;
		bool is_tainted = false;
		taint_reg_data_t taint_data;
	} register_data_t;

	std::vector<register_data_t> reg_values;

	// filled by fill_instr_details()
	unsigned long	instruction_addr = -1;
	char		instruction[MAX_INSTRUCTION] = {0};
	std::string	filename;
	std::string	func_name;
	unsigned long	func_offset = -1;

	void print_n_tabs(int tabs) {
		for(int i = 0 ; i < tabs ; i ++) {
			dr_fprintf(STDERR, "\t");
		}
	}

	void print(int tabs = 0) {
		print_n_tabs(tabs);
		dr_fprintf(STDERR, "==============================\n");
		if(chain_end) {
			for(int i = 0 ; i < tabs ; i ++) {
				dr_fprintf(STDERR, "\t");
			}
			dr_fprintf(STDERR, COLOR_RED "Taint-dependent memory access\n");
		}
		print_n_tabs(tabs);
		if(!from_read) {
			dr_fprintf(STDERR, "%p %s!" COLOR_BLUE "%s" COLOR_RESET "+%d\n",
					this->instruction_addr,
					this->filename.c_str(),
					this->func_name.c_str(),
					this->func_offset);
			print_n_tabs(tabs);

			static char pretty_instruction[STR_MAX_LEN] = {0};
			int printed;
			void *drcontext = dr_get_current_drcontext();
			disassemble_to_buffer	(drcontext,
					(byte*)this->instruction,
					(unsigned char*)instruction_addr,
					true,
					true,
					pretty_instruction,
					sizeof(pretty_instruction),
					&printed
					);
			dr_fprintf(STDERR, "%s", pretty_instruction);
		} else {
			dr_fprintf(STDERR, COLOR_YELLOW "taint source" COLOR_RESET "\n");
		}

		if(this->src_addr != ~0ul || this->dst_addr != ~0ul) {
			print_n_tabs(tabs);

			dr_fprintf(STDERR, "data (%d bytes", this->src_data.size());
			if(this->src_addr != ~0ul) {
				dr_fprintf(STDERR, " from %p", this->src_addr);
			}
			if(this->dst_addr != ~0ul) {
				dr_fprintf(STDERR, " to %p", this->dst_addr);
			}
			dr_fprintf(STDERR, "): ");

			for (char c : this->src_data) {
				dr_fprintf(STDERR, "%02x ", c);
			}
			dr_fprintf(STDERR, "\t[");
			for (char c : this->src_data) {
				dr_fprintf(STDERR, "%c", isgraph(c)?c:' ');
			}
			dr_fprintf(STDERR, "]\n");
		}
		if(this->reg_values.size() != 0) {
			for(auto x : this->reg_values) {
				print_n_tabs(tabs);
				dr_fprintf(STDERR, "%s = ", x.printable_name);
				for(size_t i = 0 ; i < x.reg_size  ; i ++) {
					dr_fprintf(STDERR, "%02lx ", x.value[i]);
				}
				dr_fprintf(STDERR, "[");
				for(size_t i = 0 ; i < x.reg_size  ; i ++) {
					char c = x.value[i];
					dr_fprintf(STDERR, "%c", isgraph(c)?c:' ');
				}
				dr_fprintf(STDERR, "]");

				if(x.is_tainted) dr_fprintf(STDERR, " (tainted)");
				dr_fprintf(STDERR, "\n");
				x.taint_data.print(2);
			}
		}
	}

	// this function fills instruction_addr, filename, func_name, func_offset
	void fill_instr_details(app_pc pc) // assuming called from a clean call
	{
		this->instruction_addr = (unsigned long)pc;

		// instruction
		void *drcontext = dr_get_current_drcontext();

		instr_t	instr;
		instr_init(drcontext, &instr);
		byte* res = decode(drcontext, (byte*)pc, &instr);
		DR_ASSERT(res != NULL);

		for(char* ins = (char*)pc; ins < (char*)res ; ins++) {
			int instr_idx = ins - (char*)pc;
			DR_ASSERT(instr_idx < MAX_INSTRUCTION && instr_idx >= 0);
			this->instruction[instr_idx] = *ins;
		}

		instr_free(drcontext, &instr);

		// symbols
		module_data_t *data;
		data = dr_lookup_module(pc);
		DR_ASSERT(data != NULL);

		this->filename = std::string(data->full_path);

		drsym_error_t symres;
		drsym_info_t sym;

		static char name[STR_MAX_LEN]; // I made it static to use less stack
		sym.struct_size = sizeof(sym);
		sym.name = name;
		sym.name_size = STR_MAX_LEN;
		static char file[STR_MAX_LEN]; // I made it static to use less stack
		sym.file = file; // can also be NULL
		sym.file_size = STR_MAX_LEN;

		symres = drsym_lookup_address(data->full_path, pc - data->start, &sym,
				DRSYM_DEFAULT_FLAGS);
		//dr_fprintf(STDERR, "symres = %d  DRSYM_ERROR_LINE_NOT_AVAILABLE = %d\n", symres,  DRSYM_ERROR_LINE_NOT_AVAILABLE);
		//dr_fprintf(STDERR, "drsym_module_has_symbols = %d\n",  drsym_module_has_symbols	(data->full_path));

		//drsym_debug_kind_t kind;
		//DR_ASSERT(DRSYM_SUCCESS == drsym_get_module_debug_kind(data->full_path, &kind));
		//dr_fprintf(STDERR, "kind = %d\n", kind);
		//DR_ASSERT(symres == DRSYM_SUCCESS);
		if(symres == DRSYM_SUCCESS || symres == DRSYM_ERROR_LINE_NOT_AVAILABLE) {
			//if(symres != DRSYM_ERROR_LINE_NOT_AVAILABLE) 
			this->func_name = std::string(sym.name);
			this->func_offset = pc - data->start - sym.start_offs;
		} else {
			this->func_name = "Symbol unavailable";
		}

		dr_free_module_data(data);
	}

	void add_reg(dr_mcontext_t* mcontext_p, reg_id_t reg, bool include_taint_data = false) {
		if(reg == DR_REG_NULL) {
			return;
		}
		register_data_t rd;
		rd.printable_name = get_register_name(reg);
		rd.reg_size = 8;
		reg_t val = reg_get_value(reg, mcontext_p);
		for(int i = 0 ; i < 8 ; i ++) {
			rd.value[i] = val & 0xff;
			val >>= 8;
		}
		if(reg_is_tainted(reg)) {
			rd.is_tainted = true;
			if(include_taint_data) {
				rd.taint_data = g_tainted_regs[reg];
			}
		}
		this->reg_values.push_back(rd);
	}
} taint_history_entry_t;

const reg_id_t g_regs_64bit[] = {
	DR_REG_RAX,
	DR_REG_RCX,
	DR_REG_RDX,
	DR_REG_RBX,
	DR_REG_RSP,
	DR_REG_RBP,
	DR_REG_RSI,
	DR_REG_RDI,
	DR_REG_R8,
	DR_REG_R9,
	DR_REG_R10,
	DR_REG_R11,
	DR_REG_R12,
	DR_REG_R13,
	DR_REG_R14,
	DR_REG_R15,
};

const reg_id_t g_regs_xmm[] = { // I put ZMMs here so I can retrieve the full data when using them in getter functions
	DR_REG_ZMM0,  DR_REG_ZMM1,  DR_REG_ZMM2,  DR_REG_ZMM3,
	DR_REG_ZMM4,  DR_REG_ZMM5,  DR_REG_ZMM6,  DR_REG_ZMM7,
	DR_REG_ZMM8,  DR_REG_ZMM9,  DR_REG_ZMM10, DR_REG_ZMM11,
	DR_REG_ZMM12, DR_REG_ZMM13, DR_REG_ZMM14, DR_REG_ZMM15,
	DR_REG_ZMM16, DR_REG_ZMM17, DR_REG_ZMM18, DR_REG_ZMM19,
	DR_REG_ZMM20, DR_REG_ZMM21, DR_REG_ZMM22, DR_REG_ZMM23,
	DR_REG_ZMM24, DR_REG_ZMM25, DR_REG_ZMM26, DR_REG_ZMM27,
	DR_REG_ZMM28, DR_REG_ZMM29, DR_REG_ZMM30, DR_REG_ZMM31
};

tag_t g_tag_allocator = 0; // a running counter for teg allocation XXX: not thread safe
std::unordered_map<void*, taint_mem_data_t> g_tainted_memory;
std::unordered_map<reg_t, taint_reg_data_t> g_tainted_regs;
std::map<tag_t, std::vector<taint_history_entry_t*>> g_taint_hisoty; // map from tags to taint history
std::set<tag_t> g_tags2print;

void mark_taint_memory(void* base, size_t size)
{
	for (size_t i = 0 ; i < size ; i ++) {
		taint_mem_data_t taint_mem;
		unsigned long tag = g_tag_allocator ++;
		char* curr_addr = (char*)base + i;
	
		for(int b = 0 ; b < 8 ; b ++) { // 8 bits per byte
			taint_mem[b] = tag;
		}

		g_tainted_memory[curr_addr] = taint_mem;

		taint_history_entry_t* taint_history_entry = new taint_history_entry_t;
		taint_history_entry->src_data.push_back(*curr_addr);
		taint_history_entry->src_addr = (unsigned long)curr_addr;
		taint_history_entry->from_read = true;
		g_taint_hisoty[tag].push_back(taint_history_entry);
	}
}

void untaint_mem(void* addr, size_t size)
{
	for(size_t i = 0 ; i < size ; i ++) {
		g_tainted_memory.erase((char*)addr + i);
	}
}

void untaint_reg(reg_id_t reg)
{
	g_tainted_regs.erase(reg);
}

bool mem_is_tainted(void* addr, size_t size)
{
	for(size_t i = 0 ; i < size ; i ++) {
		auto taint_i = g_tainted_memory.find(addr);
		if(taint_i != g_tainted_memory.end()) {
			return true;
		}
	}
	return false;
}

bool reg_is_tainted(reg_id_t reg)
{
	if(reg == DR_REG_NULL) {
		return false;
	}
	auto taint_data = g_tainted_regs.find(reg);
	if(taint_data == g_tainted_regs.end()) {
		return false;
	}
	if(taint_data->second.empty()) {
		return false;
	}
	return true;
}

/* dst_reg is the register to move to. Must be a canonical name register - R?X or xmm?
 * orig_mem_size is the size of the memory to be transferred from memory to regiter
 * mem_addr is the src address */
void taint_mem2reg(reg_id_t dst_reg, int orig_mem_size, reg_t mem_addr, taint_history_entry_t* taint_history_entry, reg_t src_reg)
{
	// XXX: figure out a way to correctly handle movzx of one byte vs mov of 1 byte etc
	// XXX: for now, I am assuming that all operations completely empty the rest of the register
	// XXX: which is wrong in cases like mov 0x1000, ax.

	// the dst reg is automatically becoming non tainted here
	taint_reg_data_t taint_reg = taint_reg_data_t((void*)mem_addr, orig_mem_size);
	std::set<tag_t> tags; // all tas involved in this instruction

	for(auto b2t : taint_reg) {
		tags.insert(b2t.second.begin(), b2t.second.end());
	}


	if(reg_is_tainted(src_reg)) { //XXX maybe can cache this reg_is_tainted query instead of repeating
		taint_reg_data_t t = g_tainted_regs[src_reg];
		for(auto b2t : t) {
			tags.insert(b2t.second.begin(), b2t.second.end());
		}
	}

	DR_ASSERT(dst_reg != DR_REG_NULL);
	if(reg_is_tainted(src_reg)) {
		g_tainted_regs[dst_reg] = taint_reg_data_t(taint_reg,  g_tainted_regs[src_reg]);
	} else {
		g_tainted_regs[dst_reg] = taint_reg;
	}

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
	}
}

void taint_mem2reg_and(reg_id_t dst_reg, int orig_mem_size, reg_t mem_addr, taint_history_entry_t* taint_history_entry, reg_t src_reg)
{
	// taking the taint bits of src_reg, and computing and with the value of the memory
	DR_ASSERT(reg_is_tainted(src_reg));
	DR_ASSERT(dst_reg != DR_REG_NULL);

	taint_reg_data_t taint = g_tainted_regs[src_reg];

	std::set<tag_t> tags; // all tas involved in this instruction

	for(int i = 0 ; i < orig_mem_size ; i ++) {
		char c = ((char*)mem_addr)[i];
		for(int b = 0 ; b < 8 ; b ++) { // b is the bit in byte
			int data_bit = (c >> b) & 1;
			if(data_bit == 0) {
				auto iterator = taint.find(b + i * 8);
				if (iterator != taint.end()) taint.erase(iterator);
			}
		}
		
	}

	for(auto b2t : taint) {
		tags.insert(b2t.second.begin(), b2t.second.end());
	}

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
	}

	g_tainted_regs[dst_reg] = taint;
}

void taint_reg2mem(void* dst_addr, size_t size, reg_id_t src_reg, taint_history_entry_t* taint_history_entry)
{
	untaint_mem(dst_addr, size); // untaint dst first

	if(!reg_is_tainted(src_reg)) {
		return;
	}

	std::set<tag_t> tags; // all tas involved in this instruction

	DR_ASSERT(src_reg != DR_REG_NULL);
	taint_reg_data_t t_reg = g_tainted_regs[src_reg];
	for(auto b2t : t_reg) {
		unsigned long byte = b2t.first / 8;
		if(byte > size) {
			continue;
		}
		int bit_in_byte = b2t.first % 8;

		void* curr_addr = (char*)dst_addr + byte;
		auto t_mem = g_tainted_memory.find(curr_addr);
		if(t_mem == g_tainted_memory.end()) {
			g_tainted_memory[curr_addr] = taint_mem_data_t();
			t_mem = g_tainted_memory.find(curr_addr);
		}
		DR_ASSERT(t_mem != g_tainted_memory.end());

		t_mem->second[bit_in_byte] = b2t.second;

		tags.insert(b2t.second.begin(), b2t.second.end());
	}

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
	}
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Syscall instrumentation ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define SYSNUM_MMAP (9)
#define SYSNUM_READ (0)

typedef struct {
	int mmap_fd; // the fd that is passed to mmap
	size_t mmap_size;

	int read_fd;
	void* read_addr;
} per_thread_t;

static int tls_idx;

static void event_thread_init(void *drcontext)
{
    /* create an instance of our data structure for this thread */
    per_thread_t *data = (per_thread_t *)dr_thread_alloc(drcontext, sizeof(per_thread_t));

    /* store it in the slot provided in the drcontext */
    drmgr_set_tls_field(drcontext, tls_idx, data);
}

bool filter_syscall_event(void *drcontext, int sysnum)
{
	// this means that we always invoke syscall 9, and sometimes filet other syscalls
	if(sysnum == SYSNUM_MMAP) {
		return true;
	}
	if(sysnum == SYSNUM_READ) {
		return true;
	}
	return false;
}

bool pre_syscall_event(void *drcontext, int sysnum)
{
	per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

	if (sysnum == SYSNUM_MMAP) {
		data->mmap_fd = dr_syscall_get_param(drcontext, 4);
		data->mmap_size = dr_syscall_get_param(drcontext, 1);
	}
	if (sysnum == SYSNUM_READ) {
		data->read_fd = dr_syscall_get_param(drcontext, 0);
		data->read_addr = (void*)dr_syscall_get_param(drcontext, 1);
	}

	return true; // true menas execute the syscall
}

void post_syscall_event(void *drcontext, int sysnum)
{
	per_thread_t *data = (per_thread_t *)drmgr_get_tls_field(drcontext, tls_idx);

	if (sysnum == SYSNUM_MMAP) {
		void* mmap_result = (void*)dr_syscall_get_result(drcontext);
		if(mmap_result == (void *) -1) {
			return; // mmap falied. We don't care about this case for a source of taint tracking
		}
		if(data->mmap_fd == -1) { // this was an mmap of an aonymous file. We don't care about these
			return;
		}
		dr_fprintf(STDERR, "created mmap at address 0x%lx of size 0x%lx\n", mmap_result, data->mmap_size);
	}
	if (sysnum == SYSNUM_READ) {
		ssize_t read_size = dr_syscall_get_result(drcontext);
		if (read_size == -1) {
			return;
		}
		
		mark_taint_memory(data->read_addr, read_size);
#if 0
		for(int i = 0 ; i < 8 ; i ++) {
			printf("%c", ((char*)data->read_addr)[i]);
		}
		printf("\n");
#endif
	}
}

static void
event_exit(void);
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data);

	DR_EXPORT void
dr_client_main(client_id_t id, int argc, const char *argv[])
{
	drreg_options_t ops = { sizeof(ops), 1 /*max slots needed: aflags*/, false }; // Marina
	dr_set_client_name("Marina's Sample Client 'Marina'",
			"http://dynamorio.org/issues");
	if (!drmgr_init())
		DR_ASSERT(false);
	drx_init();
	drreg_init(&ops); // Marina

	/* Register events: */
	dr_register_exit_event(event_exit);
	if (!drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL))
		DR_ASSERT(false);

	// syscall handling events
	dr_register_filter_syscall_event(filter_syscall_event);
	drmgr_register_pre_syscall_event(pre_syscall_event);
	drmgr_register_post_syscall_event(post_syscall_event);

	// tls for the syscall handling events
	tls_idx = drmgr_register_tls_field();
	drmgr_register_thread_init_event(event_thread_init);

	if (drsym_init(0) != DRSYM_SUCCESS) {
		dr_log(NULL, DR_LOG_ALL, 1, "WARNING: unable to initialize symbol translation\n"); // XXX replace with assert
	}

	/* Make it easy to tell from the log file which client executed. */
	dr_log(NULL, DR_LOG_ALL, 1, "Client 'Marina' initializing\n");
}

static void event_exit(void)
{
	dr_fprintf(STDERR, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n");
	dr_fprintf(STDERR, "g_tag_allocator = %ld\n", g_tag_allocator);

	for(auto t : g_tags2print) {
		if (g_taint_hisoty[t][1]->filename.find("ld") != std::string::npos) {
			continue; // don't want to print everything from the linker
		} 

		dr_fprintf(STDERR, "tag = %ld\n", t);
		for(auto e : g_taint_hisoty[t]) {
			e->print(1);
		}
	}

	if (!drmgr_unregister_bb_insertion_event(event_app_instruction))
		DR_ASSERT(false);
	drx_exit();
	drmgr_exit();
}

static void clean_call_mem2reg(app_pc pc, int opnd_size, reg_id_t dst_reg, reg_id_t src_reg)
{
	void *drcontext = dr_get_current_drcontext();

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here

	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	instr_t	instr;
	instr_init(drcontext, &instr);
	byte* res = decode(drcontext, (byte*)pc, &instr);
	DR_ASSERT(res != NULL);

	app_pc src_addr = instr_compute_address(&instr, &mcontext);
	DR_ASSERT(src_addr != NULL); 

	if(!mem_is_tainted((void*)src_addr, opnd_size) && !reg_is_tainted(src_reg)) {
		untaint_reg(dst_reg);
		return;
	}

	taint_history_entry_t* taint_history_entry = new taint_history_entry_t;
	taint_history_entry->fill_instr_details(pc);

	for(int i = 0 ; i < opnd_size ; i ++) {
		char c = ((char*)src_addr)[i];
		taint_history_entry->src_data.push_back(c);
	}

	taint_history_entry->src_addr = (unsigned long)src_addr;

	if(src_reg != DR_REG_NULL) {
		taint_history_entry->add_reg(&mcontext, src_reg);
	}

	taint_mem2reg(dst_reg, opnd_size, (unsigned long)src_addr, taint_history_entry, src_reg);

	instr_free(drcontext, &instr);
}

static void clean_call_mem2reg_and(app_pc pc, int opnd_size, reg_id_t dst_reg, reg_id_t src_reg)
{
	void *drcontext = dr_get_current_drcontext();

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here

	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	instr_t	instr;
	instr_init(drcontext, &instr);
	byte* res = decode(drcontext, (byte*)pc, &instr);
	DR_ASSERT(res != NULL);

	app_pc src_addr = instr_compute_address(&instr, &mcontext);
	DR_ASSERT(src_addr != NULL); 

	if(!mem_is_tainted((void*)src_addr, opnd_size) && !reg_is_tainted(src_reg)) {
		untaint_reg(dst_reg);
		return;
	}

	taint_history_entry_t* taint_history_entry = new taint_history_entry_t;
	taint_history_entry->fill_instr_details(pc);

	for(int i = 0 ; i < opnd_size ; i ++) {
		char c = ((char*)src_addr)[i];
		taint_history_entry->src_data.push_back(c);
	}

	taint_history_entry->src_addr = (unsigned long)src_addr;

	if(src_reg != DR_REG_NULL) {
		taint_history_entry->add_reg(&mcontext, src_reg);
	}

	if(reg_is_tainted(src_reg)) {
		taint_mem2reg_and(dst_reg, opnd_size, (unsigned long)src_addr, taint_history_entry, src_reg);
	} else { // if src_reg is not tainted treat and as all other opcodes
		taint_mem2reg(dst_reg, opnd_size, (unsigned long)src_addr, taint_history_entry, src_reg);
	}

	instr_free(drcontext, &instr);
}

static void clean_call_untaint_mem(int orig_mem_size, app_pc pc)
{
	// if I got here, the instruction has a single memory operand
	void *drcontext = dr_get_current_drcontext();

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here

	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	instr_t	instr;
	instr_init(drcontext, &instr);
	byte* res = decode(drcontext, (byte*)pc, &instr);
	DR_ASSERT(res != NULL);
	
	app_pc dst_addr = instr_compute_address(&instr, &mcontext);
	DR_ASSERT(dst_addr != NULL);
	untaint_mem(dst_addr, orig_mem_size);

	instr_free(drcontext, &instr);
}

static void clean_call_untaint_reg(reg_id_t reg)
{
	untaint_reg(reg);
}

static void clean_call_reg2mem(int orig_mem_size, app_pc pc, reg_t src_reg)
{
	// if I got here, the instruction has a single 64 reg src and a single memory dst operand
	void *drcontext = dr_get_current_drcontext();

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL;

	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	instr_t	instr;
	instr_init(drcontext, &instr);
	byte* res = decode(drcontext, (byte*)pc, &instr);
	DR_ASSERT(res != NULL);

	app_pc dst_addr = instr_compute_address(&instr, &mcontext);
	DR_ASSERT(dst_addr != NULL); // TODO: return this

	if(!reg_is_tainted(src_reg)) {
		untaint_mem(dst_addr, orig_mem_size);
	} else { // src_reg is tainted - need to propogate taint
		taint_history_entry_t* taint_history_entry = new taint_history_entry_t;
		taint_history_entry->fill_instr_details(pc);

		byte src_data[64];
		reg_get_value_ex(src_reg, &mcontext, src_data);	
		for(int i = 0 ; i < orig_mem_size ; i ++) {
			taint_history_entry->src_data.push_back(src_data[i]);
		}
		taint_history_entry->dst_addr = (unsigned long)dst_addr;
		taint_reg2mem(dst_addr, orig_mem_size, src_reg, taint_history_entry);
	}
	instr_free(drcontext, &instr);
}

static void clean_call_check_taint2addr(app_pc pc, reg_id_t reg0, reg_id_t reg1, reg_id_t reg2)
{
	if(!reg_is_tainted(reg0) && !reg_is_tainted(reg1) && !reg_is_tainted(reg2)) {
		return;
	}

	const reg_id_t regs[] = {reg0, reg1, reg2};
	std::set<tag_t> tags; // all tas involved in this instruction
	for(int i = 0 ; i < 3 ; i ++) {
		auto taint_data = g_tainted_regs.find(regs[i]);
		if(taint_data == g_tainted_regs.end()) {
			continue;
		}
		//DR_ASSERT(regs[i] != DR_REG_NULL); // XXX - why does this fail?
		for(auto b2t : taint_data->second) {
			tags.insert(b2t.second.begin(), b2t.second.end());
		}
	}

	void *drcontext = dr_get_current_drcontext();

	taint_history_entry_t* taint_history_entry = new taint_history_entry_t;
	taint_history_entry->chain_end = true;
	taint_history_entry->fill_instr_details(pc);

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here

	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	for(int i = 0 ; i < 3 ; i ++) {
		if(regs[i] == DR_REG_NULL) break;
		taint_history_entry->add_reg(&mcontext, regs[i], true);
	}

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
		g_tags2print.insert(t);
	}
}

static void clean_call_taint_reg2reg(app_pc pc, int opcode,
		reg_id_t src_reg0, reg_id_t src_reg1, reg_id_t src_reg2, reg_id_t src_reg3,
		reg_id_t dst_reg0, reg_id_t dst_reg1, reg_id_t dst_reg2, reg_id_t dst_reg3, reg_id_t dst_reg4)
{
	std::set<reg_id_t> src_regs {src_reg0, src_reg1, src_reg2, src_reg3};
	std::set<reg_id_t> dst_regs = {dst_reg0, dst_reg1, dst_reg2, dst_reg3, dst_reg4};
	src_regs.erase(DR_REG_NULL);
	dst_regs.erase(DR_REG_NULL);

	int cnt_tainted_srcs = 0;
	for(auto s : src_regs) {
		if(reg_is_tainted(s)) {
			cnt_tainted_srcs ++;
		}
	}
	if(cnt_tainted_srcs == 0) {
		// if no tainted src, untaint dst
		for(auto d : dst_regs) {
			untaint_reg(d);
		}
		return;
	}

	// adding to taint_history_table before actually propogating taint
	std::set<tag_t> tags;
	for(auto s : src_regs) {
		taint_reg_data_t taint_data = g_tainted_regs[s];
		for(auto b2t : taint_data) {
			tags.insert(b2t.second.begin(), b2t.second.end());
		}
	}

	taint_history_entry_t* taint_history_entry = new taint_history_entry_t();
	taint_history_entry->fill_instr_details(pc);

	void *drcontext = dr_get_current_drcontext();

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here
	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	for(auto s : src_regs) {
		if(!reg_is_xmm(s)) { // XXX: not supporting xmm values
			taint_history_entry->add_reg(&mcontext, s);
		}
		
	}

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
	}


	// now going to actually propogate taint

	taint_reg_data_t new_merged_taint_data;
	for(auto s : src_regs) {
		taint_reg_data_t taint_data = g_tainted_regs[s];
		new_merged_taint_data = taint_reg_data_t(new_merged_taint_data, taint_data);
	}

	for(auto d : dst_regs) {
		DR_ASSERT(d != DR_REG_NULL);
		g_tainted_regs[d] = new_merged_taint_data; // TODO: take size of operation into account
	}
	return;
}

static void clean_call_taint_reg2reg_shift_imm(app_pc pc, int opcode, reg_id_t reg, ptr_int_t imm, opnd_size_t reg_size)
{
	DR_ASSERT(opcode == OP_shl || opcode == OP_sal || opcode == OP_sar || opcode == OP_shr);
	if(!reg_is_tainted(reg)) {
		return;
	}
	taint_reg_data_t orig_taint_data = g_tainted_regs[reg];
	taint_reg_data_t new_taint_data;

	int nr_bits;
	if(reg_size == OPSZ_1) nr_bits = 1 * 8;
	if(reg_size == OPSZ_2) nr_bits = 2 * 8;
	if(reg_size == OPSZ_4) nr_bits = 4 * 8;
	if(reg_size == OPSZ_8) nr_bits = 8 * 8;


	for(auto b2t : orig_taint_data) {
		if(opcode == OP_shl || opcode == OP_sal) {
			if(b2t.first + imm < nr_bits) {
				new_taint_data[b2t.first + imm] = b2t.second;
			}
		} else { /* oppcode == OP_sar */
			DR_ASSERT(opcode == OP_sar || opcode == OP_shr);
			if(b2t.first - imm >= 0) {
				new_taint_data[b2t.first - imm] = b2t.second;
			}

		}
	}


	// add to log
	std::set<tag_t> tags;
	for(auto b2t : new_taint_data) {
		tags.insert(b2t.second.begin(), b2t.second.end());
	}

	taint_history_entry_t* taint_history_entry = new taint_history_entry_t();
	taint_history_entry->fill_instr_details(pc);

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here
	dr_get_mcontext(dr_get_current_drcontext(), &mcontext);

	taint_history_entry->add_reg(&mcontext, reg);

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
	}

	// update taint
	g_tainted_regs[reg] = new_taint_data;
}

static void clean_call_lea(app_pc pc, int opcode, reg_id_t base, int scale, reg_id_t index, reg_id_t dst_reg)
{
	// XXX I am assuming that the registes here are in the 64 bith form. Hopefullt don't have to assert that
	if(!reg_is_tainted(base) && !reg_is_tainted(index)) {
		untaint_reg(dst_reg);
		return;
	}

	// base_taint
	taint_reg_data_t base_taint;

	auto t = g_tainted_regs.find(base);
	if(t != g_tainted_regs.end()) {
		base_taint = t->second;;
	}

	// index_taint
	taint_reg_data_t index_taint;
	t = g_tainted_regs.find(index);
	if(t != g_tainted_regs.end()) {
		index_taint = t->second;
	}
	if(scale) {
		index_taint = taint_reg_data_t::shift_left_ctor(index_taint, log2(scale), 64);
	}
	taint_reg_data_t res_taint = taint_reg_data_t(base_taint, index_taint);

	// create taint history entery
	std::set<tag_t> tags;
	for(auto b2t : res_taint) {
		tags.insert(b2t.second.begin(), b2t.second.end());
	}

	taint_history_entry_t* taint_history_entry = new taint_history_entry_t();
	taint_history_entry->fill_instr_details(pc);

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here
	dr_get_mcontext(dr_get_current_drcontext(), &mcontext);

	taint_history_entry->add_reg(&mcontext, base);
	taint_history_entry->add_reg(&mcontext, index);

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
	}


	//update taint
	g_tainted_regs[dst_reg] = res_taint;
}

static void clean_call_taint_reg2reg_shift_reg(app_pc pc, int opcode, reg_id_t reg_amount, reg_id_t reg, opnd_size_t reg_size)
{ // TODO: use the shift left and right ctors
	DR_ASSERT(opcode == OP_shl || opcode == OP_sal || opcode == OP_sar || opcode == OP_shr);
	if(!reg_is_tainted(reg)) {
		// XXX ignoring the case where the shift amount register is tainted
		return;
	}
	taint_reg_data_t orig_taint_data = g_tainted_regs[reg];
	taint_reg_data_t new_taint_data;

	unsigned int nr_bits;
	if(reg_size == OPSZ_1) nr_bits = 1 * 8;
	if(reg_size == OPSZ_4) nr_bits = 4 * 8;
	if(reg_size == OPSZ_8) nr_bits = 8 * 8;

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here
	dr_get_mcontext(dr_get_current_drcontext(), &mcontext);

	reg_t shift_amount = reg_get_value(reg_amount, &mcontext);

	for(auto b2t : orig_taint_data) {
		if(opcode == OP_shl || opcode == OP_sal) {
			if(b2t.first + shift_amount < nr_bits) {
				new_taint_data[b2t.first + shift_amount] = b2t.second;
			}
		} else { /* oppcode == OP_sar */
			if(b2t.first - shift_amount >= 0) {
				new_taint_data[b2t.first - shift_amount] = b2t.second;
			}

		}
	}

	g_tainted_regs[reg] = new_taint_data;

	// add to log
	std::set<tag_t> tags;
	for(auto b2t : new_taint_data) {
		tags.insert(b2t.second.begin(), b2t.second.end());
	}

	taint_history_entry_t* taint_history_entry = new taint_history_entry_t();
	taint_history_entry->fill_instr_details(pc);


	taint_history_entry->add_reg(&mcontext, reg);
	taint_history_entry->add_reg(&mcontext, reg_amount);

	for(auto t : tags) {
		g_taint_hisoty[t].push_back(taint_history_entry);
	}
}

// this is for instruction of the form `push R_X'
static void clean_call_taint_push_reg(app_pc pc, reg_id_t src_reg)
{
	void *drcontext = dr_get_current_drcontext();

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here

	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	instr_t	instr;
	instr_init(drcontext, &instr);
	byte* res = decode(drcontext, (byte*)pc, &instr);
	DR_ASSERT(res != NULL);

	app_pc dst_addr = instr_compute_address(&instr, &mcontext); // XXX for efficieny, can probably get the offset to RSP a priori
	DR_ASSERT(dst_addr != NULL); // TODO: return this

	if(!reg_is_tainted(src_reg)) {
		untaint_mem(dst_addr, 8);
	} else { // src_reg is tainted - need to propogate taint
		taint_history_entry_t* taint_history_entry = new taint_history_entry_t;
		taint_history_entry->fill_instr_details(pc);

		byte src_data[8];
		reg_get_value_ex(src_reg, &mcontext, src_data);	
		for(int i = 0 ; i < 8 ; i ++) {
			taint_history_entry->src_data.push_back(src_data[i]);
		}
		taint_history_entry->dst_addr = (unsigned long)dst_addr;
		taint_reg2mem(dst_addr, 8, src_reg, taint_history_entry);
	}
	instr_free(drcontext, &instr);
}

// this is for instruction of the form `pop R_X'
static void clean_call_taint_pop_reg(app_pc pc, reg_id_t dst_reg)
{
	void *drcontext = dr_get_current_drcontext();

	dr_mcontext_t mcontext;
	mcontext.size = sizeof(mcontext);
	mcontext.flags = DR_MC_ALL; // XXX maybe can cut on xmm flags here

	bool ret = dr_get_mcontext(drcontext, &mcontext);
	DR_ASSERT(ret);

	instr_t	instr;
	instr_init(drcontext, &instr);
	byte* res = decode(drcontext, (byte*)pc, &instr);
	DR_ASSERT(res != NULL);

	app_pc src_addr = instr_compute_address(&instr, &mcontext); // XXX for efficieny, can probably get the offset to RSP a priori
	DR_ASSERT(src_addr != NULL);

	if(!mem_is_tainted(src_addr, 8)) {
		untaint_reg(dst_reg);
	} else {
		taint_history_entry_t* taint_history_entry = new taint_history_entry_t;
		taint_history_entry->fill_instr_details(pc);

		for(int i = 0 ; i < 8 ; i ++) {
			char c = ((char*)src_addr)[i];
			taint_history_entry->src_data.push_back(c);
		}

		taint_history_entry->src_addr = (unsigned long)src_addr;
		taint_mem2reg(dst_reg, 8, (reg_t)src_addr, taint_history_entry, DR_REG_NULL);
	}
	instr_free(drcontext, &instr);
}
	    
/* This is called separately for each instruction in the block. */
static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data)
{
	drmgr_disable_auto_predication(drcontext, bb);

/////////////////////////////////////////////////////////
///////// Detect taint dependent memory access //////////
/////////////////////////////////////////////////////////

	std::set<reg_id_t> regs; // to hold the registers that are used for address calculation

	if(instr_reads_memory(instr)) { // need this check to filter out lea
		for(int i = 0 ; i < instr_num_srcs(instr) ; i ++) {
			opnd_t opnd = instr_get_src(instr, i);	
			if(opnd_is_memory_reference(opnd)) {
				reg_id_t reg = DR_REG_NULL;
				for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
					if(opnd_uses_reg(opnd, g_regs_64bit[i])) {
						reg = g_regs_64bit[i];
						regs.insert(reg);
					}
				}
				// only consider the GP 64 bits register

			}
		}
	}
	if(instr_writes_memory(instr)) {
		for(int i = 0 ; i < instr_num_dsts(instr) ; i ++) {
			opnd_t opnd = instr_get_dst(instr, i);	
			if(opnd_is_memory_reference(opnd)) {
				reg_id_t reg = DR_REG_NULL;
				for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
					if(opnd_uses_reg(opnd, g_regs_64bit[i])) {
						reg = g_regs_64bit[i];
						regs.insert(reg);
					}
				}
			}

		}
	}
	DR_ASSERT(regs.size() <= 3); // I am only counting the 64 bit registers. No segment. XMM should be impossible. Empirically never get more than 3
	if(!regs.empty()) {
		reg_id_t reg0 = DR_REG_NULL;
		reg_id_t reg1 = DR_REG_NULL;
		reg_id_t reg2 = DR_REG_NULL;

		std::vector<reg_id_t> v(regs.begin(), regs.end());
		DR_ASSERT(v.size() > 0 && v.size() <= 3);
		if(v.size() > 0) {
			reg0 = v[0];
		}
		if(v.size() > 1) {
			reg1 = v[1];
		}
		if(v.size() > 2) {
			reg2 = v[2];
		}
		
		// isert a clean call
		dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_check_taint2addr, false, 4,
			OPND_CREATE_INT64(instr_get_app_pc(instr)),
			OPND_CREATE_INT32(reg0),
			OPND_CREATE_INT32(reg1),
			OPND_CREATE_INT32(reg2)
		);
	}


/////////////////////////////////////////////////////////
//////////////// Propogate taint ////////////////////////
/////////////////////////////////////////////////////////

	if (instr_reads_memory(instr) && !instr_writes_memory(instr) && instr_num_dsts(instr) == 1) {
		if(instr_get_opcode(instr) == OP_ret) {
			goto out;
		}
		DR_ASSERT(instr_num_srcs(instr) <= 2); // one memory operand and one reg

		//dr_print_instr(drcontext, STDERR, instr, "hello\t");
		// all abouut dst reg
		opnd_t dst_opnd = instr_get_dst(instr, 0);
		DR_ASSERT(opnd_is_reg(dst_opnd));

		reg_id_t dst_reg = DR_REG_NULL;
		for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
			if(opnd_uses_reg(dst_opnd, g_regs_64bit[i])) {
				dst_reg = g_regs_64bit[i];
			}
		}
		for (reg_id_t i = 0 ; i < SIZEOF(g_regs_xmm) ; i ++) {
			if(opnd_uses_reg(dst_opnd, g_regs_xmm[i])) {
				dst_reg = g_regs_xmm[i];
			}
		}
		DR_ASSERT(dst_reg != DR_REG_NULL); // TODO: return this assrt after fixing avx case

		// all about opnd size
		opnd_t src_opnd = instr_get_src(instr, 0);
		uint opnd_size = instr_memory_reference_size(instr);

		// all about additional src reg. Assuming is is a GP register and not XMM
		reg_id_t src_reg = DR_REG_NULL;
		if(instr_num_srcs(instr) == 2) {
			opnd_t src_reg_opnd = instr_get_src(instr, 1);
			if (!opnd_is_reg(src_reg_opnd)) {
				goto out; // one example instruction here is  vpcmpeqb %ymm0 (%rdi)[32byte] -> %ymm1. Ignoring these
			}
			for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
				if(opnd_uses_reg(src_reg_opnd, g_regs_64bit[i])) {
					src_reg = g_regs_64bit[i];
				}
			}
		}

		// isert a clean call
		if(instr_get_opcode(instr) != OP_and) {
			dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_mem2reg, false, 4,
					OPND_CREATE_INT64(instr_get_app_pc(instr)),
					OPND_CREATE_INT32(opnd_size),
					OPND_CREATE_INT32(dst_reg),
					OPND_CREATE_INT64(src_reg)
					);
		} else { // OP_and
			dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_mem2reg_and, false, 4,
					OPND_CREATE_INT64(instr_get_app_pc(instr)),
					OPND_CREATE_INT32(opnd_size),
					OPND_CREATE_INT32(dst_reg),
					OPND_CREATE_INT64(src_reg)
					);

					}

#if 0
		dr_print_opnd (drcontext, STDERR, lea_opnd, "lea_opnd");
		dr_print_opnd (drcontext, STDERR, orig_mem_opnd, "orig_mem_opnd");
		dr_print_instr(drcontext, STDERR, instr, "hello\t");
		dr_print_instr(drcontext, STDERR, lea_instr, "lea\t");
		dr_fprintf(STDERR, "instr_num_srcs = %d\t, instr_num_dsts = %d\n", instr_num_srcs(instr), instr_num_dsts(instr));
#endif

	}
	else if (!instr_reads_memory(instr) && instr_writes_memory(instr) && instr_num_dsts(instr) == 1) {

		ptr_int_t value;
		// src could be either nothing - setz, or register or immediate
		if(instr_num_srcs(instr) < 1 || instr_is_mov_constant(instr, &value)) {
			// there are 0 srcs in instructions like setz.s
			// XXX: Taint could go through FLAGS, but ignoring this case for now
			// XXX: for optimisation, I can treat differently dst addresses that depend on regiters and ones that do not, and compute the patter one here instead of inside the clean call.

			uint orig_mem_size = instr_memory_reference_size(instr);

			dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_untaint_mem, false, 2,
					OPND_CREATE_INT32(orig_mem_size),
					OPND_CREATE_INT64(instr_get_app_pc(instr))
					);

			return DR_EMIT_DEFAULT;
		}
#if 0
		dr_print_instr(drcontext, STDERR, instr, "hello\t");
#endif
		//DR_ASSERT(instr_is_mov(instr)); // all instructions here seem to be mov, but this assertion fails on muovups

		DR_ASSERT(instr_num_srcs(instr));
		opnd_t src_opnd = instr_get_src(instr, 0);
		DR_ASSERT(opnd_is_reg(src_opnd));

		// if I am here, the instruction if mov register->memory address
		// going to try and convert all GP registers to their 32 bit version
		//src_opnd = opnd_shrink_to_32_bits(src_opnd);

		// this is the most elegant way I found to convert registers to their corresponding 64 bit form
		reg_id_t src_reg = DR_REG_NULL;
		for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
			if(opnd_uses_reg(src_opnd, g_regs_64bit[i])) {
				src_reg = g_regs_64bit[i];
			}
		}
		for (reg_id_t i = 0 ; i < SIZEOF(g_regs_xmm) ; i ++) {
			if(opnd_uses_reg(src_opnd, g_regs_xmm[i])) {
				src_reg = g_regs_xmm[i];
			}
		}

		dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_reg2mem, false, 3,
				OPND_CREATE_INT32(instr_memory_reference_size(instr)),
				OPND_CREATE_INT64(instr_get_app_pc(instr)),
				OPND_CREATE_INT64(src_reg)
				);

	} else if(instr_num_dsts(instr) == 0) {
		// these are instrucions like cmp, test, jz, nop
	} else if (!instr_reads_memory(instr) && !instr_writes_memory(instr)) {
		// these are operations that involve registers and immediates
		// I am looking at them as a pile of src and dst registers
		// XXX src could be only immediate

		std::set<reg_id_t> src_regs;
		std::set<reg_id_t> dst_regs;

		for(int i = 0 ; i < instr_num_srcs(instr) ; i ++) {
			opnd_t opnd = instr_get_src(instr, i);	
			reg_id_t reg = DR_REG_NULL;
			for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
				if(opnd_uses_reg(opnd, g_regs_64bit[i])) {
					reg = g_regs_64bit[i];
					src_regs.insert(reg);
				}
			}
			for (reg_id_t i = 0 ; i < SIZEOF(g_regs_xmm) ; i ++) {
				if(opnd_uses_reg(opnd, g_regs_xmm[i])) {
					reg = g_regs_xmm[i];
					src_regs.insert(reg);
				}
			}
		}
		for(int i = 0 ; i < instr_num_dsts(instr) ; i ++) {
			opnd_t opnd = instr_get_dst(instr, i);	
			reg_id_t reg = DR_REG_NULL;
			for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
				if(opnd_uses_reg(opnd, g_regs_64bit[i])) {
					reg = g_regs_64bit[i];
					dst_regs.insert(reg);
				}
			}
			for (reg_id_t i = 0 ; i < SIZEOF(g_regs_xmm) ; i ++) {
				if(opnd_uses_reg(opnd, g_regs_xmm[i])) {
					reg = g_regs_xmm[i];
					dst_regs.insert(reg);
				}
			}
		}

		if(src_regs.size() == 0) {
			for(auto r : dst_regs) {
				// this covers instructions like move immediate into register, lea of immediate (or immediate relative to rip) into register, setz, rdtsc
				//dr_print_instr(drcontext, STDERR, instr, "hello\t");
				dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_untaint_reg, false, 1,
						OPND_CREATE_INT32(r)
						);
			}
		} else {
			// I am treating these as one pile of registers
			// moving taint to another pile of registers
			// Not sure yet what to do if there are multiple
			// different taint sources in one instruction XXX
			DR_ASSERT(src_regs.size() < 4); // empirically tested
			DR_ASSERT(dst_regs.size() < 5); // empirically tested

			reg_id_t src_reg0 = DR_REG_NULL;
			reg_id_t src_reg1 = DR_REG_NULL;
			reg_id_t src_reg2 = DR_REG_NULL;
			reg_id_t src_reg3 = DR_REG_NULL;

			std::vector<reg_id_t> src_v(src_regs.begin(), src_regs.end());
			DR_ASSERT(src_v.size() > 0 && src_v.size() < 4);
			if(src_v.size() > 0) src_reg0 = src_v[0];
			if(src_v.size() > 1) src_reg1 = src_v[1];
			if(src_v.size() > 2) src_reg2 = src_v[2];
			if(src_v.size() > 3) src_reg3 = src_v[3];

			reg_id_t dst_reg0 = DR_REG_NULL;
			reg_id_t dst_reg1 = DR_REG_NULL;
			reg_id_t dst_reg2 = DR_REG_NULL;
			reg_id_t dst_reg3 = DR_REG_NULL;
			reg_id_t dst_reg4 = DR_REG_NULL;

			std::vector<reg_id_t> dst_v(dst_regs.begin(), dst_regs.end());
			DR_ASSERT(dst_v.size() > 0 && dst_v.size() < 5);
			if(dst_v.size() > 0) dst_reg0 = dst_v[0];
			if(dst_v.size() > 1) dst_reg1 = dst_v[1];
			if(dst_v.size() > 2) dst_reg2 = dst_v[2];
			if(dst_v.size() > 3) dst_reg3 = dst_v[3];
			if(dst_v.size() > 4) dst_reg4 = dst_v[4];

			int opcode = instr_get_opcode(instr);

			if((opcode == OP_shl || opcode == OP_sal || opcode == OP_sar || opcode == OP_shr) && src_regs.size() == 1 && dst_regs.size() == 1 && src_reg0 == dst_reg0) {
				opnd_t opnd_imm = instr_get_src(instr, 0);	
				DR_ASSERT(opnd_is_immed(opnd_imm));
				ptr_int_t imm = opnd_get_immed_int(opnd_imm);
				opnd_size_t reg_size = opnd_get_size(instr_get_src(instr, 1));
				DR_ASSERT(reg_size == OPSZ_1 || reg_size == OPSZ_2 || reg_size == OPSZ_4 || reg_size == OPSZ_8);

				dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_taint_reg2reg_shift_imm, false, 5,
						OPND_CREATE_INT64(instr_get_app_pc(instr)),
						OPND_CREATE_INT32(opcode),
						OPND_CREATE_INT32(src_reg0),
						OPND_CREATE_INT64(imm),
						OPND_CREATE_INT32(reg_size)
						);
			} else if((opcode == OP_xor || opcode == OP_pxor) && src_regs.size() == 1 && dst_regs.size() == 1 && src_reg0 == dst_reg0) {
				dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_untaint_reg, false, 1,
						OPND_CREATE_INT32(dst_reg0)
						);
			} else if(opcode == OP_shl || opcode == OP_sal || opcode == OP_sar || opcode == OP_shr) {
				// shr and sar that use %cl as the number of bits to shift
				DR_ASSERT(opnd_is_reg(instr_get_src(instr, 0)));
				DR_ASSERT(opnd_get_size(instr_get_src(instr, 0)) == OPSZ_1);
				opnd_size_t reg_size = opnd_get_size(instr_get_src(instr, 1));
				DR_ASSERT(reg_size == OPSZ_1 || reg_size == OPSZ_4 || reg_size == OPSZ_8);

				dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_taint_reg2reg_shift_reg, false, 5,
						OPND_CREATE_INT64(instr_get_app_pc(instr)),
						OPND_CREATE_INT32(opcode),
						OPND_CREATE_INT32(src_reg0),
						OPND_CREATE_INT32(src_reg1),
						OPND_CREATE_INT32(reg_size)
						);
			} else if (opcode == OP_lea) {
				opnd_t opnd = instr_get_src(instr, 0);
				int scale = opnd_get_scale(opnd);
				reg_id_t base = opnd_get_base(opnd);
				reg_id_t index = opnd_get_index(opnd);

				if(scale == 0) DR_ASSERT(index == DR_REG_NULL);

				DR_ASSERT(dst_regs.size() == 1);
				dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_lea, false, 6,
						OPND_CREATE_INT64(instr_get_app_pc(instr)),
						OPND_CREATE_INT32(opcode),
						OPND_CREATE_INT32(base),
						OPND_CREATE_INT32(scale),
						OPND_CREATE_INT32(index),
						OPND_CREATE_INT32(dst_reg0)
						);


			} else {
				dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_taint_reg2reg, false, 11,
						OPND_CREATE_INT64(instr_get_app_pc(instr)),
						OPND_CREATE_INT32(opcode),
						OPND_CREATE_INT32(src_reg0),
						OPND_CREATE_INT32(src_reg1),
						OPND_CREATE_INT32(src_reg2),
						OPND_CREATE_INT32(src_reg3),
						OPND_CREATE_INT32(dst_reg0),
						OPND_CREATE_INT32(dst_reg1),
						OPND_CREATE_INT32(dst_reg2),
						OPND_CREATE_INT32(dst_reg3),
						OPND_CREATE_INT32(dst_reg4)
						);
			}
		}
	} else if (instr_get_opcode(instr) == OP_push && !instr_reads_memory(instr)) {
		DR_ASSERT(instr_num_srcs(instr) == 2);
		opnd_t src_opnd = instr_get_src(instr, 0);
		DR_ASSERT(opnd_is_reg(src_opnd));

		reg_id_t src_reg = DR_REG_NULL;
		for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
			if(opnd_uses_reg(src_opnd, g_regs_64bit[i])) {
				src_reg = g_regs_64bit[i];
			}
		}
		DR_ASSERT(src_reg != DR_REG_NULL);
		DR_ASSERT(opnd_get_reg(src_opnd) == src_reg);

		dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_taint_push_reg, false, 2,
				OPND_CREATE_INT64(instr_get_app_pc(instr)),
				OPND_CREATE_INT64(src_reg)
				);
	} else if (instr_get_opcode(instr) == OP_pop) {
		DR_ASSERT(!instr_writes_memory(instr));

		DR_ASSERT(instr_num_dsts(instr) == 2);
		opnd_t dst_opnd = instr_get_dst(instr, 0);
		DR_ASSERT(opnd_is_reg(dst_opnd));

		reg_id_t dst_reg = DR_REG_NULL;
		for (reg_id_t i = 0 ; i < SIZEOF(g_regs_64bit) ; i ++) {
			if(opnd_uses_reg(dst_opnd, g_regs_64bit[i])) {
				dst_reg = g_regs_64bit[i];
			}
		}
		DR_ASSERT(dst_reg != DR_REG_NULL);
		DR_ASSERT(opnd_get_reg(dst_opnd) == dst_reg);
		DR_ASSERT(dst_reg != DR_REG_RSP);

		dr_insert_clean_call(drcontext, bb, instr, (void*)clean_call_taint_pop_reg, false, 2,
				OPND_CREATE_INT64(instr_get_app_pc(instr)),
				OPND_CREATE_INT64(dst_reg)
				);
	} else {
		// this case seems to be only push, pop, call and instructions that modify
		// memory in place like or of a memory location into a memory location.
		// I guess that string operations could also be here.
#if 0
		dr_print_instr(drcontext, STDERR, instr, "hello\t");
#endif
	}

out:
	return DR_EMIT_DEFAULT;

}
