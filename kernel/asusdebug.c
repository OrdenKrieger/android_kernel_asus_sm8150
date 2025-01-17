#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/rtc.h>
#include <linux/list.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/export.h>
#include <linux/module.h>

#ifdef FORCE_WD_RESET_FEATURE
#include <linux/input/qpnp-power-on.h>
extern void kernel_restart(char *cmd);
extern int get_download_mode(void);
extern int g_watchdog_reboot;
#endif

#define SZ_4M 0x00400000

int entering_suspend = 0;
char messages[256];
extern char evtlog_bootup_reason[50];
char evtlog_poweroff_reason[50];

#ifdef CONFIG_PON_EVT_LOG
char evtlog_pon_dump[100];
#endif
extern int ASUSEvt_poweroff_reason;

static const char * const ASUSEvt_poweroff_reason_str[] = {
	/* QPNP_PON_GEN1 POFF reasons */
	[0] = "[Soft reset]",
	[1] = "[Reset via PS_HOLD]",
	[2] = "[PMIC Watchdog]",
	[3] = "[Keypad_Reset1]",
	[4] = "[Keypad_Reset2]",
	[5] = "[Simultaneous power key and reset line]",
	[6] = "[Reset line/Volume Down Key]",
	[7] = "[Power Key]",
	[8] = "[N/A]",
	[9] = "[N/A]",
	[10] = "[N/A]",
	[11] = "[Charger]",
	[12] = "[N/A]",
	[13] = "[UVLO]",
	[14] = "[PMIC Overtemp]",
	[15] = "[Fail safe reset]",

	/* QPNP_PON_GEN2 FAULT reasons */
	[16] = "Triggered from GP_FAULT0",
	[17] = "Triggered from GP_FAULT1",
	[18] = "Triggered from GP_FAULT2",
	[19] = "Triggered from GP_FAULT3",
	[20] = "Triggered from MBG_FAULT",
	[21] = "Triggered from OVLO (Over Voltage Lock Out)",
	[22] = "Triggered from UVLO (Under Voltage Lock Out)",
	[23] = "Triggered from AVDD_RB",
	[24] = "N/A",
	[25] = "N/A",
	[26] = "N/A",
	[27] = "Triggered from FAULT_FAULT_N",
	[28] = "Triggered from FAULT_PBS_WATCHDOG_TO",
	[29] = "Triggered from FAULT_PBS_NACK",
	[30] = "Triggered from FAULT_RESTART_PON",
	[31] = "Triggered from OTST3 (Over Temperature)",

	/* QPNP_PON_GEN2 S3_RESET reasons */
	[32] = "N/A",
	[33] = "N/A",
	[34] = "N/A",
	[35] = "N/A",
	[36] = "Triggered from S3_RESET_FAULT_N",
	[37] = "Triggered from S3_RESET_PBS_WATCHDOG_TO",
	[38] = "Triggered from S3_RESET_PBS_NACK",
	[39] = "Triggered from S3_RESET_KPDPWR_ANDOR_RESIN",
};

extern int nSuspendInProgress;
static struct workqueue_struct *ASUSEvtlog_workQueue;
static int g_hfileEvtlog = -MAX_ERRNO;
static int g_bEventlogEnable = 1;
static char g_Asus_Eventlog[ASUS_EVTLOG_MAX_ITEM][ASUS_EVTLOG_STR_MAXLEN];
static int g_Asus_Eventlog_read = 0;
static int g_Asus_Eventlog_write = 0;

static void do_write_event_worker(struct work_struct *work);
static DECLARE_WORK(eventLog_Work, do_write_event_worker);

static struct mutex mA;
#define AID_SDCARD_RW 1015
static void do_write_event_worker(struct work_struct *work)
{
	char buffer[256];
	memset(buffer, 0, sizeof(char)*256);

	if (IS_ERR((const void *)(long int)g_hfileEvtlog)) {
		long size;

		g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
		sys_chown(ASUS_EVTLOG_PATH".txt", AID_SDCARD_RW, AID_SDCARD_RW);

		size = sys_lseek(g_hfileEvtlog, 0, SEEK_END);
		if (size >= SZ_4M) {
			sys_close(g_hfileEvtlog);
			sys_rename(ASUS_EVTLOG_PATH "_old.txt", ASUS_EVTLOG_PATH "_old1.txt");
			sys_rmdir(ASUS_EVTLOG_PATH"_old.txt");
			sys_rename(ASUS_EVTLOG_PATH".txt", ASUS_EVTLOG_PATH"_old.txt");
			g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
		}
		if (ASUSEvt_poweroff_reason < 0)
			strcpy(evtlog_poweroff_reason, "[Power lost][Unknown]");
		else
			strcpy(evtlog_poweroff_reason, ASUSEvt_poweroff_reason_str[ASUSEvt_poweroff_reason]);

		sprintf(buffer, "\n\n---------------System Boot----%s---------\n"
			"[Shutdown] Power off Reason: 0x%x => %s; (last time) ######\n"
			"###### Bootup Reason: %s ######\n",
			ASUS_SW_VER,
			(ASUSEvt_poweroff_reason < 0) ? 0 : 1 << ASUSEvt_poweroff_reason,
			evtlog_poweroff_reason, evtlog_bootup_reason);

		sys_write(g_hfileEvtlog, buffer, strlen(buffer));

	#ifdef CONFIG_PON_EVT_LOG
		sys_fsync(g_hfileEvtlog);
		snprintf(buffer, sizeof(buffer),
		        "[PON Dump] %s \n",evtlog_pon_dump);
		sys_write(g_hfileEvtlog, buffer, strlen(buffer));
	#endif

	#ifdef FORCE_WD_RESET_FEATURE
		if(!get_download_mode()) {
			if(g_watchdog_reboot) {
				qpnp_pon_system_pwr_off(PON_POWER_OFF_HARD_RESET);
				sprintf(buffer, "Force hard reset because of PMIC abnormal reboot reason.\n");
				sys_write(g_hfileEvtlog, buffer, strlen(buffer));
			}
		}
	#endif

		sys_close(g_hfileEvtlog);
	}
	if (!IS_ERR((const void *)(long int)g_hfileEvtlog)) {
		int str_len;
		char *pchar;
		long size;

		g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
		sys_chown(ASUS_EVTLOG_PATH".txt", AID_SDCARD_RW, AID_SDCARD_RW);

		size = sys_lseek(g_hfileEvtlog, 0, SEEK_END);
		if (size >= SZ_4M) {
			sys_close(g_hfileEvtlog);
			sys_rename(ASUS_EVTLOG_PATH "_old.txt", ASUS_EVTLOG_PATH "_old1.txt");
			sys_rmdir(ASUS_EVTLOG_PATH"_old.txt");
			sys_rename(ASUS_EVTLOG_PATH".txt", ASUS_EVTLOG_PATH"_old.txt");
			g_hfileEvtlog = sys_open(ASUS_EVTLOG_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
		}

		while (g_Asus_Eventlog_read != g_Asus_Eventlog_write) {
			mutex_lock(&mA);
			str_len = strlen(g_Asus_Eventlog[g_Asus_Eventlog_read]);
			pchar = g_Asus_Eventlog[g_Asus_Eventlog_read];
			g_Asus_Eventlog_read++;
			g_Asus_Eventlog_read %= ASUS_EVTLOG_MAX_ITEM;
			mutex_unlock(&mA);

			if (pchar[str_len - 1] != '\n') {
				if(str_len + 1 >= ASUS_EVTLOG_STR_MAXLEN)
					str_len = ASUS_EVTLOG_STR_MAXLEN - 2;
				pchar[str_len] = '\n';
				pchar[str_len + 1] = '\0';
			}

			sys_write(g_hfileEvtlog, pchar, strlen(pchar));
			sys_fsync(g_hfileEvtlog);

		#ifdef FORCE_WD_RESET_FEATURE
			if(!get_download_mode()) {
				if(g_watchdog_reboot) {
					char rebootbuffer[256];
					printk("PMIC abnormal warm reset case. Force to hard reset.");
					msleep(1000);
					memset(rebootbuffer, 0, sizeof(char)*256);
					sprintf(rebootbuffer, "abnormalreset");
					kernel_restart(rebootbuffer);
				}
			}
		#endif
		}
		sys_close(g_hfileEvtlog);
	}
}

extern struct timezone sys_tz;

void ASUSEvtlog(const char *fmt, ...)
{

	va_list args;
	char *buffer;

	if (g_bEventlogEnable == 0)
		return;
	if (!in_interrupt() && !in_atomic() && !irqs_disabled())
		mutex_lock(&mA);

	buffer = g_Asus_Eventlog[g_Asus_Eventlog_write];
	g_Asus_Eventlog_write++;
	g_Asus_Eventlog_write %= ASUS_EVTLOG_MAX_ITEM;

	if (!in_interrupt() && !in_atomic() && !irqs_disabled())
		mutex_unlock(&mA);

	memset(buffer, 0, ASUS_EVTLOG_STR_MAXLEN);
	if (buffer) {
		struct rtc_time tm;
		struct timespec ts;

		getnstimeofday(&ts);
		ts.tv_sec -= sys_tz.tz_minuteswest * 60;
		rtc_time_to_tm(ts.tv_sec, &tm);
		getrawmonotonic(&ts);
		sprintf(buffer, "(%ld)%04d-%02d-%02d %02d:%02d:%02d :", ts.tv_sec, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		/*printk(buffer);*/
		va_start(args, fmt);
		vscnprintf(buffer + strlen(buffer), ASUS_EVTLOG_STR_MAXLEN - strlen(buffer), fmt, args);
		va_end(args);
		/*printk(buffer);*/
		queue_work(ASUSEvtlog_workQueue, &eventLog_Work);
	} else {
		printk("ASUSEvtlog buffer cannot be allocated\n");
	}
}
EXPORT_SYMBOL(ASUSEvtlog);

static ssize_t evtlogswitch_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if(strncmp(buf, "0", 1) == 0) {
		ASUSEvtlog("ASUSEvtlog disable !!");
		printk("ASUSEvtlog disable !!\n");
		flush_work(&eventLog_Work);
		g_bEventlogEnable = 0;
	}
	if (strncmp(buf, "1", 1) == 0) {
		g_bEventlogEnable = 1;
		ASUSEvtlog("ASUSEvtlog enable !!");
		printk("ASUSEvtlog enable !!\n");
	}

	return count;
}
static ssize_t asusevtlog_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	if (count > 256)
		count = 256;

	memset(messages, 0, sizeof(messages));
	if (copy_from_user(messages, buf, count))
		return -EFAULT;

	ASUSEvtlog("%s", messages);

	return count;
}

static int asusdebug_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int asusdebug_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t asusdebug_read(struct file *file, char __user *buf,
size_t count, loff_t *ppos)
{
	return 0;
}

extern int rtc_ready;
static ssize_t asusdebug_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	u8 messages[256] = {0};
	int nLength = strlen("panic");

	if (count > 256)
		count = 256;
	if (copy_from_user(messages, buf, count))
		return -EFAULT;

	if (strncmp(messages, "panic", nLength) == 0) {
		panic("panic test");
	}

	return count;
}

static const struct file_operations proc_evtlogswitch_operations = {
	.write	  = evtlogswitch_write,
};
static const struct file_operations proc_asusevtlog_operations = {
	.write	  = asusevtlog_write,
};
static const struct file_operations proc_asusdebug_operations = {
	.read	   = asusdebug_read,
	.write	  = asusdebug_write,
	.open	   = asusdebug_open,
	.release	= asusdebug_release,
};
static const struct file_operations proc_asusdebugprop_operations = {
	.read	   = asusdebug_read,
	.write	  = asusdebug_write,
	.open	   = asusdebug_open,
	.release	= asusdebug_release,
};

int g_QPST_property = 0;
extern void set_QPSTInfo_dloadmode(int mode);

static ssize_t QPSTInfo_read(struct file *file, char __user *buf,
			      size_t count, loff_t *ppos)
{
	char print_buf[128];
	unsigned int ret = 0, iret = 0;

	sprintf(print_buf, "[QPST] getproperty persist.sys.downloadmode.enable = %d\n", g_QPST_property);
	ret = strlen(print_buf);
	iret = copy_to_user(buf, print_buf, ret);

	return 0;
}

static ssize_t QPSTInfo_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	u8 messages[256] = { 0 };

	if (count > 256)
		count = 256;
	if (copy_from_user(messages, buf, count))
		return -EFAULT;

	if (strncmp(messages, "0", strlen("0")) == 0) {
		g_QPST_property = 0;
	} else if (strncmp(messages, "1", strlen("1")) == 0) {
		g_QPST_property = 1;
	}
	printk("[QPST] setproperty persist.sys.downloadmode.enable = %d\n", g_QPST_property);

	/*set download_mode when writing QPSTInfo*/
	set_QPSTInfo_dloadmode(g_QPST_property);

	return count;
}

static const struct file_operations proc_QPSTInfo_operations = {
	.read		= QPSTInfo_read,
	.write		= QPSTInfo_write,
};

/* Coredump */
int g_coredump_property = 0;
static ssize_t coredump_read(struct file *file, char __user *buf,
			      size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t coredump_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	u8 messages[256] = { 0 };

	if (count > 256)
		count = 256;
	if (copy_from_user(messages, buf, count))
		return -EFAULT;

	if (strncmp(messages, "0", strlen("0")) == 0) {
		g_coredump_property = 0;
	} else if (strncmp(messages, "1", strlen("1")) == 0) {
		g_coredump_property = 1;
	}
	printk("[Debug][Coredump] g_coredump_property = %d\n", g_coredump_property);

	return count;
}

static const struct file_operations proc_coredump_operations = {
	.read		= coredump_read,
	.write		= coredump_write,
};

/*AS-K SubSys Health Record+++*/
static char g_SubSys_W_Buf[SUBSYS_W_MAXLEN];
static char g_SubSys_C_Buf[SUBSYS_C_MAXLEN]="0000-0000-0000-0000-0000";
static void do_write_subsys_worker(struct work_struct *work);
static void do_count_subsys_worker(struct work_struct *work);
static void do_delete_subsys_worker(struct work_struct *work);
static DECLARE_WORK(subsys_w_Work, do_write_subsys_worker);
static DECLARE_WORK(subsys_c_Work, do_count_subsys_worker);
static DECLARE_WORK(subsys_d_Work, do_delete_subsys_worker);
static struct completion SubSys_C_Complete;

static void do_write_subsys_worker(struct work_struct *work)
{
	int hfile = -MAX_ERRNO;
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	hfile = sys_open(SUBSYS_HEALTH_MEDICAL_TABLE_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
	if(!IS_ERR((const void *)(long int)hfile)) {
		if (sys_lseek(hfile, 0, SEEK_END) >= SZ_128K) {
			ASUSEvtlog("[SSR-Info] SubSys is versy ill\n");
			sys_close(hfile);
			sys_unlink(SUBSYS_HEALTH_MEDICAL_TABLE_PATH"_old.txt");
			sys_rename(SUBSYS_HEALTH_MEDICAL_TABLE_PATH".txt", SUBSYS_HEALTH_MEDICAL_TABLE_PATH"_old.txt");
			hfile = sys_open(SUBSYS_HEALTH_MEDICAL_TABLE_PATH".txt", O_CREAT|O_RDWR|O_SYNC, 0666);
		}
		sys_write(hfile, g_SubSys_W_Buf, strlen(g_SubSys_W_Buf));
		sys_fsync(hfile);
		sys_close(hfile);
	} else {
		ASUSEvtlog("[SSR-Info] Save SubSys Medical Table Error: [0x%x]\n", hfile);
		sys_unlink(SUBSYS_HEALTH_MEDICAL_TABLE_PATH".txt");/*Delete The File Which is Opened in Address Space Mismatch*/
	}
	set_fs(old_fs);
}

static void do_count_subsys_worker(struct work_struct *work)
{
	int  hfile = -MAX_ERRNO;
	char r_buf[SUBSYS_R_MAXLEN];
	int  r_size = 0;
	int  index = 0;
	char keys[] = "[SSR]:";
	char *pch;
	char n_buf[64];
	char SubSysName[SUBSYS_NUM_MAX][10];
	int  Counts[SUBSYS_NUM_MAX] = { 0 };
	int  subsys_num = 0;
	char OutSubSysName[3][10] = { "modem", "no_wifi", "adsp" };/* Confirm SubSys Name for Each Platform */
	int  OutCounts[4] = { 0 };/* MODEM, WIFI, ADSP, OTHERS */
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* Search All SubSys Supported */
	for(index = 0 ; index < SUBSYS_NUM_MAX ; index++) {
		sprintf(n_buf, SUBSYS_BUS_ROOT"/subsys%d/name", index);
		hfile = sys_open(n_buf, O_RDONLY|O_SYNC, 0444);
		if(!IS_ERR((const void *)(long int)hfile)) {
			memset(r_buf, 0, sizeof(r_buf));
			r_size = sys_read(hfile, r_buf, sizeof(r_buf));
			if (r_size > 0) {
				sprintf(SubSysName[index], r_buf, r_size-2);/* Skip \n\0 */
				SubSysName[index][r_size-1] = '\0';/* Insert \0 at last */
				subsys_num++;
			}
			sys_close(hfile);
		}
	}

	hfile = sys_open(SUBSYS_HEALTH_MEDICAL_TABLE_PATH".txt", O_CREAT|O_RDONLY|O_SYNC, 0444);
	if(!IS_ERR((const void *)(long int)hfile)) {
		do {
			memset(r_buf, 0, sizeof(r_buf));
			r_size = sys_read(hfile, r_buf, sizeof(r_buf));
			if (r_size != 0) {
				/* count */
				pch = strstr(r_buf, keys);
				while (pch != NULL) {
					pch = pch + strlen(keys);
					for (index = 0 ; index < subsys_num ; index++) {
						if (!strncmp(pch, SubSysName[index], strlen(SubSysName[index]))) {
							Counts[index]++;
							break;
						}
					}
					pch = strstr(pch, keys);
				}
			}
		} while (r_size != 0);

		sys_close(hfile);
	}

	hfile = sys_open(SUBSYS_HEALTH_MEDICAL_TABLE_PATH"_old.txt", O_RDONLY|O_SYNC, 0444);
	if(!IS_ERR((const void *)(long int)hfile)) {
		do {
			memset(r_buf, 0, sizeof(r_buf));
			r_size = sys_read(hfile, r_buf, sizeof(r_buf));
			if (r_size != 0) {
				/* count */
				pch = strstr(r_buf, keys);
				while (pch != NULL) {
					pch = pch + strlen(keys);
					for (index = 0 ; index < subsys_num ; index++) {
						if (!strncmp(pch, SubSysName[index], strlen(SubSysName[index]))) {
							Counts[index]++;
							break;
						}
					}
					pch = strstr(pch, keys);
				}
			}
		} while (r_size != 0);

		sys_close(hfile);
	}

	/* Map The Out Pattern */
	for(index = 0 ; index < subsys_num ; index++) {
		if (!strncmp(OutSubSysName[0], SubSysName[index], strlen(SubSysName[index]))) {
			OutCounts[0] += Counts[index]; /* MODEM */
		} else if (!strncmp(OutSubSysName[1], SubSysName[index], strlen(SubSysName[index]))) {
			OutCounts[1] += Counts[index]; /* WIFI */
		} else if (!strncmp(OutSubSysName[2], SubSysName[index], strlen(SubSysName[index]))) {
			OutCounts[2] += Counts[index]; /* ADSP */
		} else {
			OutCounts[3] += Counts[index]; /* OTHERS */
		}
	}

	set_fs(old_fs);

	sprintf(g_SubSys_C_Buf, "%s-%d-%d-%d-%d", r_buf, OutCounts[0], OutCounts[1], OutCounts[2], OutCounts[3]);/* MODEM, WIFI, ADSP, OTHERS */
	complete(&SubSys_C_Complete);
}

static void do_delete_subsys_worker(struct work_struct *work)
{
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	sys_unlink(SUBSYS_HEALTH_MEDICAL_TABLE_PATH".txt");
	sys_unlink(SUBSYS_HEALTH_MEDICAL_TABLE_PATH"_old.txt");
	set_fs(old_fs);
	ASUSEvtlog("[SSR-Info] SubSys Medical Table Deleted\n");
}

void SubSysHealthRecord(const char *fmt, ...)
{
	va_list args;
	char *w_buf;
	struct rtc_time tm;
	struct timespec ts;

	memset(g_SubSys_W_Buf, 0 , sizeof(g_SubSys_W_Buf));
	w_buf = g_SubSys_W_Buf;

	getnstimeofday(&ts);
	ts.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(ts.tv_sec, &tm);
	sprintf(w_buf, "%04d-%02d-%02d %02d:%02d:%02d : ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	va_start(args, fmt);
	vscnprintf(w_buf + strlen(w_buf), sizeof(g_SubSys_W_Buf) - strlen(w_buf), fmt, args);
	va_end(args);
	/*printk("g_SubSys_W_Buf = %s", g_SubSys_W_Buf);*/

	queue_work(ASUSEvtlog_workQueue, &subsys_w_Work);
}
EXPORT_SYMBOL(SubSysHealthRecord);

static int SubSysHealth_proc_show(struct seq_file *m, void *v)
{
	unsigned long ret;

	queue_work(ASUSEvtlog_workQueue, &subsys_c_Work);/* Issue to count */

	ret = wait_for_completion_timeout(&SubSys_C_Complete, msecs_to_jiffies(1000));
	if (!ret)
		ASUSEvtlog("[SSR-Info] Timed out on query SubSys count\n");

	seq_printf(m, "%s\n", g_SubSys_C_Buf);
	return 0;
}

static int SubSysHealth_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, SubSysHealth_proc_show, NULL);
}

static ssize_t SubSysHealth_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char keyword[] = "clear";
	char tmpword[10];
	memset(tmpword, 0, sizeof(tmpword));

	/* no data be written Or Input size is too large to write our buffer */
	if ((!count) || (count > (sizeof(tmpword) - 1)))
		return -EINVAL;

	if (copy_from_user(tmpword, buf, count))
		return -EFAULT;

	if (strncmp(tmpword, keyword, strlen(keyword)) == 0) {
		queue_work(ASUSEvtlog_workQueue, &subsys_d_Work);
	}

	return count;
}

static const struct file_operations proc_SubSysHealth_operations = {
	.open = SubSysHealth_proc_open,
	.read = seq_read,
	.write = SubSysHealth_proc_write,
};
/*AS-K SubSys Health Record---*/

static int __init proc_asusdebug_init(void)
{

	proc_create("asusdebug", S_IALLUGO, NULL, &proc_asusdebug_operations);
	proc_create("asusdebug-prop", S_IALLUGO, NULL, &proc_asusdebugprop_operations);
	proc_create("asusevtlog", S_IRWXUGO, NULL, &proc_asusevtlog_operations);
	proc_create("asusevtlog-switch", S_IRWXUGO, NULL, &proc_evtlogswitch_operations);
	proc_create("QPSTInfo", S_IRWXUGO, NULL, &proc_QPSTInfo_operations);
	proc_create("coredump", S_IRWXUGO, NULL, &proc_coredump_operations);
	mutex_init(&mA);

	ASUSEvtlog_workQueue  = create_singlethread_workqueue("ASUSEVTLOG_WORKQUEUE");

	/*AS-K SubSys Health Record+++*/
	proc_create("SubSysHealth", S_IRWXUGO, NULL, &proc_SubSysHealth_operations);
	init_completion(&SubSys_C_Complete);
	/*AS-K SubSys Health Record---*/

	return 0;
}
module_init(proc_asusdebug_init);
