// This header gets force included in all C compiles including for pico-sdk files so we can define a macro that calls functions here

#ifdef __cplusplus
extern "C" 
#else
extern 
#endif
const char *get_net_hostname();

