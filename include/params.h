#ifndef _PARAMS_H_
#define _PARAMS_H_

typedef enum
{
	PERM_USER,
	PERM_MANUFACTURER
} PARAM_PERM;

typedef enum
{
	PARAM_INT,
	PARAM_STR
} PARAM_TYPE;

#define FLAG_USER			1
#define FLAG_FACTORY		2
#define FLAG_TEMP			4

typedef struct tagParamDesc {
	char * name;
	int get_pri;
	int set_pri;
	PARAM_PERM perm;
	PARAM_TYPE type;
	unsigned char flag;
	int i_value;
	char * s_value;
	int factory_i_value;
	char * factory_s_value;
	int default_i_value;
	char * default_s_value;
	int min_value;
	int max_value;
	int min_length;
	int max_length;
	bool (* check_valid)(struct tagParamDesc * desc, const char * value);
	int temp_i_value;
	char * temp_s_value;
	struct list_head list;
} PARAM_DESC;

typedef struct tagPropertyDesc {
	char * name;
	PARAM_TYPE type;
	int i_value;
	char * s_value;
	struct list_head list;
} PROPERTY_DESC;

typedef struct tagParamsContext
{
	pthread_mutex_t mutex;
	PARAM_DESC * params;
	unsigned int params_number;
	struct list_head params_list[256];
	PROPERTY_DESC * properties;
	unsigned int properties_number;
	struct list_head properties_list[256];
} PARAMS_CONTEXT;

void init_params_context(PARAMS_CONTEXT * c, PARAM_DESC * params, unsigned int params_number, PROPERTY_DESC * properties, unsigned int properties_number);

bool set_property_int_byindex(PARAMS_CONTEXT * c, unsigned int index, int value);

bool set_property_str_byindex(PARAMS_CONTEXT * c, unsigned int index, const char * value);

const char * get_property_str_byindex(PARAMS_CONTEXT * c, unsigned int index);

int get_property_int_byindex(PARAMS_CONTEXT * c, unsigned int index);

int get_property_js_byname(PARAMS_CONTEXT * c, char * buffer, const char * name, const char * alias);

int get_all_property_js(PARAMS_CONTEXT * c, char * buffer);

bool set_param_int_byname(PARAMS_CONTEXT * c, const char * name, int value, PARAM_PERM perm, unsigned char flag, int pri);

bool set_param_str_byname(PARAMS_CONTEXT * c, const char * name, const char * value, PARAM_PERM perm, unsigned char flag, int pri);

bool set_param_int_byindex(PARAMS_CONTEXT * c, unsigned int index, int value, PARAM_PERM perm, unsigned char flag, int pri);

bool set_param_addr_byindex(PARAMS_CONTEXT * c, unsigned int index, unsigned long value, PARAM_PERM perm, unsigned char flag, int pri);

bool set_param_str_byindex(PARAMS_CONTEXT * c, unsigned int index, const char * value, PARAM_PERM perm, unsigned char flag, int pri);

char * get_param_str_byindex(PARAMS_CONTEXT * c, unsigned int index, unsigned char flag, int pri);

int get_param_int_byindex(PARAMS_CONTEXT * c, unsigned int index, unsigned char flag, int pri);

int get_param_int_byname(PARAMS_CONTEXT * c, const char * name, unsigned char flag, int pri);

char * get_param_str_byname(PARAMS_CONTEXT * c, const char * name, unsigned char flag, int pri);

unsigned long get_param_addr_byindex(PARAMS_CONTEXT * c, unsigned int index, unsigned char flag, int pri);

int get_param_js_byname(PARAMS_CONTEXT * c, char * buffer, const char * name, const char * alias, unsigned char flag, int pri);

int get_all_params_js(PARAMS_CONTEXT * c, char * buffer, unsigned char flag, int pri);

void temp_to_params(PARAMS_CONTEXT * c, unsigned char flag);

void free_temp_params(PARAMS_CONTEXT * c);

bool check_common_param_valid(struct tagParamDesc * desc, const char * value);

bool check_tz_valid(struct tagParamDesc * desc, const char * value);

bool check_ip_valid(struct tagParamDesc * desc, const char * value);

bool check_mask_valid(struct tagParamDesc * desc, const char * value);

bool check_resolution_valid(struct tagParamDesc * desc, const char * value);

bool check_mac_valid(struct tagParamDesc * desc, const char * value);

#endif
