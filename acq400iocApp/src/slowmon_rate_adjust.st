/* slowmon_rate_adjust.st
 * Set sets desired slowmon output rate.
 * */

program slowmon_rate_adjust
option +r;

string sync_role; assign sync_role 	to "{uut}:SYS:SYNC_ROLE";  monitor sync_role;
int intclk_hz;	  assign intclk_hz	to "{uut}:{s1}:INTCLK_HZ"; monitor intclk_hz;

int slowmon_fs;   assign slowmon_fs 	to "{uut}:0:SLOWMON:FS"; monitor slowmon_fs;
int slowmon_fin;  assign slowmon_fin 	to "{uut}:0:SLOWMON:FIN";
int slowmon_fin_act;  assign slowmon_fin_act 	to "{uut}:0:SLOWMON:FIN:ACT"; 

int nacc_nacc; 	  assign nacc_nacc 	to "{uut}:0:NACC:NACC"; monitor nacc_nacc;
int nacc_start;	  assign nacc_start 	to "{uut}:0:NACC:START";
int nacc_shr;	  assign nacc_shr 	to "{uut}:0:NACC:SHR";
int nacc_pres;	  assign nacc_pres 	to "{uut}:0:NACC:PRES";
int NACC_PRES_LIM;assign NACC_PRES_LIM 	to "{uut}:0:NACC:PRES.DRVH";

#define NACC_LIM	256

int SLOWMON_FIN_MAX; assign SLOWMON_FIN_MAX to "{uut}:0:SLOWMON:FIN.DRVH";

%{
void write_internal_role(int _intclk_hz)
{
	FILE* fp = fopen("/dev/shm/role", "w");
	if (fp == 0){
		perror("/dev/shm/role");
	}else{
		fprintf(fp, "internal %d", _intclk_hz);
		fclose(fp);
	}
}
int getfs(char* _sync_role, int _intclk_hz) {
	int _fs;
	char mult = '\0';
	
	if (strcmp(_sync_role, "role not set") == 0 || strstr(_sync_role, "internal")){
		write_internal_role(_intclk_hz);
		return _intclk_hz;
	}else if (sscanf(_sync_role, "%*s %d%c", &_fs, &mult) >= 1){
		switch (mult){
		case 'M':
			return _fs*1000000;
		case 'k':
			return _fs*1000;
		default:
			return _fs;
		}
	}else{
		return 66666;	
	}	
}

}%


string sync_role0 = { "not set" };


int slowmon_fs0 = -1;
int nacc_nacc0 = 0;
int fs;

int verbose;


ss slowmon_rate_adjust {
	state init {
		when() {
			if (macValueGet("verbose")){
				verbose = macValueGet("verbose")[0] - '0';
			}
			pvGet(NACC_PRES_LIM);
			pvGet(sync_role);
			pvGet(intclk_hz);
			pvGet(SLOWMON_FIN_MAX);
			pvGet(nacc_nacc);
			nacc_nacc0 = nacc_nacc;

			if (macValueGet("verbose")){
				printf("sync_role \"%s\"\n", sync_role);
			}
			fs = getfs(sync_role, intclk_hz);
			if (fs == 0){
				printf("ERROR: fs==0 sync_role:%s\n", sync_role);
				fs=100000;
			}
		} state wait_change
	} 
	
	state make_global_change {
		entry {
			if (verbose){
				printf("make_change\n");
			}
		}
		when() {
			if (slowmon_fs == 0){
				nacc_nacc = nacc_pres = nacc_shr = slowmon_fin = 0;
			}else if (fs > NACC_PRES_LIM * NACC_LIM * SLOWMON_FIN_MAX){
				printf("ERRROR: FS %d is too fast for available decimation: %d\n", fs, NACC_PRES_LIM * NACC_LIM * SLOWMON_FIN_MAX);
				nacc_pres = NACC_PRES_LIM;
				nacc_nacc = NACC_LIM;
				slowmon_fin = SLOWMON_FIN_MAX;
			}else{
				/* first, minimize software oversampling */ 
				slowmon_fin = fs / (NACC_PRES_LIM * NACC_LIM);
				if (slowmon_fin > SLOWMON_FIN_MAX){
					slowmon_fin = SLOWMON_FIN_MAX;
				}else if (slowmon_fin < slowmon_fs){
					slowmon_fin = slowmon_fs;
				}
				
				/* next, minimize prescale */
				for (nacc_pres  = NACC_PRES_LIM; 
				     nacc_pres>1 && fs < nacc_pres * NACC_LIM * slowmon_fin;
				     nacc_pres >>= 1){
					if (verbose){
						printf("nacc_pres %d   fs %d < %d\n", nacc_pres, fs, nacc_pres * NACC_LIM * slowmon_fin);
					}
				}
				for (nacc_nacc = NACC_LIM;
				     nacc_nacc>1 && fs < nacc_pres * nacc_nacc * slowmon_fin;
				     nacc_nacc >>= 1){
					if (verbose){
						printf("nacc_nacc %d   fs %d < %d\n", nacc_nacc, fs, nacc_pres * nacc_nacc * slowmon_fin);
					}
				}
			}

			if (slowmon_fs > slowmon_fin){
				slowmon_fs = slowmon_fin;
				pvPut(slowmon_fs);
			}
			pvPut(slowmon_fin);

		} state make_nacc_change
	}

	state make_nacc_change {
		entry {
			if (verbose){
				printf("wait_change\n");
			}
		}
		when() {
			for (nacc_shr = 0; 1<<nacc_shr < nacc_nacc; nacc_shr++){
							;
			}
			slowmon_fin_act = fs/(nacc_pres * nacc_nacc);
			pvPut(slowmon_fin_act);
			pvPut(nacc_shr);
			pvPut(nacc_pres);
			pvPut(nacc_nacc);
			nacc_nacc0 = nacc_nacc;
		} state wait_change
	}

	state wait_change {
		entry {
			if (verbose){
				printf("wait_change\n");
			}
		}
		when(slowmon_fs != slowmon_fs0 || strcmp(sync_role, sync_role0)) {
			strcpy(sync_role0, sync_role);
			fs = getfs(sync_role, intclk_hz);
			slowmon_fs0 = slowmon_fs;
		} state make_global_change

		when(nacc_nacc != nacc_nacc0){

		} state make_nacc_change
		
	}
}
