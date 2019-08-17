int maemo_init(int *argc, char ***argv);
void maemo_finish();

extern char file_name[MAXPATHLEN];
extern int g_maemo_opts;

extern inline void key_press_event(int key,int type);

typedef struct
{ 
	int sens;
	int y_def;
	float maxValue;
	float xMultiplier;
	float yMultiplier;
} accel_option;

extern accel_option accelOptions;
