
extern u8 zero_mem[0x1000];

void new_dyna_pcsx_mem_init(void);
void new_dyna_pcsx_mem_reset(void);
void new_dyna_pcsx_mem_load_state(void);
void new_dyna_pcsx_mem_shutdown(void);

int pcsxmem_is_handler_dynamic(u_int addr);
