#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/ioctl.h>

#define I2C_BUS_AVAILABLE   (          1 )
#define SLAVE_DEVICE_NAME   ( "TABI_OLED" )
#define SSD1306_SLAVE_ADDR  (       0x3C )
#define SSD1306_MAX_SEG         (        128 )
#define SSD1306_MAX_LINE        (          7 )
#define SSD1306_DEF_FONT_SIZE   (          5 )
#define GPIO_SERVO 25
#define GPIO_BUTTON 5

#define TIME_HIGH_OPEN 500000
#define TIME_HIGH_CLOSE 1500000
#define TIME_PERIOD 20000000

#define MY_IOCTL_MAGIC 'a'
#define WR_DOOR_STA _IOW (MY_IOCTL_MAGIC, 'a', int)
#define RD_DOOR_STA _IOR (MY_IOCTL_MAGIC, 'b', int)
#define WR_C_PASS _IOW (MY_IOCTL_MAGIC, 'd', char[6])
#define RD_C_PASS _IOR (MY_IOCTL_MAGIC, 'e', char[6])

dev_t dev = 0;
static struct class *tabi_class;
static struct cdev tabi_cdev;
static int t_door_status;

static unsigned long last_jiffies = 0;

static struct hrtimer hrtimer;
static ktime_t time_high_open;
static ktime_t time_high_close;
static ktime_t time_period;
static int pwm_state = 0;
static atomic_t door_status = ATOMIC_INIT(0);

static char c_pass[6] = {'1','2','3','4','5','6'};
static char pass[6];
static atomic_t c_pos = ATOMIC_INIT(0);

static int gpios[8] = {12,16,20,21,6,13,19,26};
static int irq_nums[5];
static char matrix[4][4] = {
    {'1','2','3','C'},
    {'4','5','6','C'},
    {'7','8','9','C'},
    {'N','0','N','N'}
};
static unsigned long now;
static atomic_t row = ATOMIC_INIT(0);
static int cols;
static struct workqueue_struct *my_wq;
static struct delayed_work my_work;

static int tabi_open(struct inode *inode, struct file *file)
{
    pr_info("Device File Opened...!!!\n");
    return 0;
}
static int tabi_release(struct inode *inode, struct file *file)
{
    pr_info("Device File Closed...!!!\n");
    return 0;
}
static ssize_t tabi_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
    pr_info("Read Function\n");
    return 0;
}
static ssize_t tabi_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
    pr_info("Write function\n");
    return len;
}
static long tabi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    if (!arg)
        return -EINVAL;
    switch (cmd)
    {
        case WR_DOOR_STA:
            if(copy_from_user(&t_door_status ,(int*) arg, sizeof(t_door_status)))
            {
                pr_err("Door status Write : Err!\n");
            }
            atomic_set(&door_status, t_door_status);
            break;
        case RD_DOOR_STA:
            t_door_status = atomic_read(&door_status);
            if(copy_to_user((int*) arg, &t_door_status, sizeof(t_door_status)))
            {
                pr_err("Door status Read : Err!\n");
            }
            break;
        case WR_C_PASS:
            if(copy_from_user(c_pass ,(char*) arg, sizeof(c_pass)))
            {
                pr_err("Pass Write : Err!\n");
            }
            break;
        case RD_C_PASS:
            if(copy_to_user((char*) arg, c_pass, sizeof(c_pass)))
            {
                pr_err("Pass Read : Err!\n");
            }
            break;
        default:
            pr_info("default\n");
            return -EINVAL;
    }
    return 0;
}
static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = tabi_read,
        .write          = tabi_write,
        .open           = tabi_open,
        .unlocked_ioctl = tabi_ioctl,
        .release        = tabi_release,
};


static struct i2c_adapter *i2c_adapter;
static struct i2c_client  *i2c_client_oled;
static uint8_t SSD1306_LineNum   = 0;
static uint8_t SSD1306_CursorPos = 0;
static uint8_t SSD1306_FontSize  = SSD1306_DEF_FONT_SIZE;
static int oled_status = 0;
static int oled_c_status;
static int head_sta = 1;
static struct workqueue_struct *oled_wq;
static struct work_struct oled_work;
static const unsigned char SSD1306_font[][SSD1306_DEF_FONT_SIZE]=
{
    {0x00, 0x00, 0x00, 0x00, 0x00},   // space
    {0x00, 0x00, 0x2f, 0x00, 0x00},   // !
    {0x00, 0x07, 0x00, 0x07, 0x00},   // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14},   // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12},   // $
    {0x23, 0x13, 0x08, 0x64, 0x62},   // %
    {0x36, 0x49, 0x55, 0x22, 0x50},   // &
    {0x00, 0x05, 0x03, 0x00, 0x00},   // '
    {0x00, 0x1c, 0x22, 0x41, 0x00},   // (
    {0x00, 0x41, 0x22, 0x1c, 0x00},   // )
    {0x14, 0x08, 0x3E, 0x08, 0x14},   // *
    {0x08, 0x08, 0x3E, 0x08, 0x08},   // +
    {0x00, 0x00, 0xA0, 0x60, 0x00},   // ,
    {0x08, 0x08, 0x08, 0x08, 0x08},   // -
    {0x00, 0x60, 0x60, 0x00, 0x00},   // .
    {0x20, 0x10, 0x08, 0x04, 0x02},   // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E},   // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00},   // 1
    {0x42, 0x61, 0x51, 0x49, 0x46},   // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31},   // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10},   // 4
    {0x27, 0x45, 0x45, 0x45, 0x39},   // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30},   // 6
    {0x01, 0x71, 0x09, 0x05, 0x03},   // 7
    {0x36, 0x49, 0x49, 0x49, 0x36},   // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E},   // 9
    {0x00, 0x36, 0x36, 0x00, 0x00},   // :
    {0x00, 0x56, 0x36, 0x00, 0x00},   // ;
    {0x08, 0x14, 0x22, 0x41, 0x00},   // <
    {0x14, 0x14, 0x14, 0x14, 0x14},   // =
    {0x00, 0x41, 0x22, 0x14, 0x08},   // >
    {0x02, 0x01, 0x51, 0x09, 0x06},   // ?
    {0x32, 0x49, 0x59, 0x51, 0x3E},   // @
    {0x7C, 0x12, 0x11, 0x12, 0x7C},   // A
    {0x7F, 0x49, 0x49, 0x49, 0x36},   // B
    {0x3E, 0x41, 0x41, 0x41, 0x22},   // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C},   // D
    {0x7F, 0x49, 0x49, 0x49, 0x41},   // E
    {0x7F, 0x09, 0x09, 0x09, 0x01},   // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A},   // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F},   // H
    {0x00, 0x41, 0x7F, 0x41, 0x00},   // I
    {0x20, 0x40, 0x41, 0x3F, 0x01},   // J
    {0x7F, 0x08, 0x14, 0x22, 0x41},   // K
    {0x7F, 0x40, 0x40, 0x40, 0x40},   // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},   // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F},   // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E},   // O
    {0x7F, 0x09, 0x09, 0x09, 0x06},   // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E},   // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46},   // R
    {0x46, 0x49, 0x49, 0x49, 0x31},   // S
    {0x01, 0x01, 0x7F, 0x01, 0x01},   // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F},   // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F},   // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F},   // W
    {0x63, 0x14, 0x08, 0x14, 0x63},   // X
    {0x07, 0x08, 0x70, 0x08, 0x07},   // Y
    {0x61, 0x51, 0x49, 0x45, 0x43},   // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00},   // [
    {0x55, 0xAA, 0x55, 0xAA, 0x55},   // Backslash (Checker pattern)
    {0x00, 0x41, 0x41, 0x7F, 0x00},   // ]
    {0x04, 0x02, 0x01, 0x02, 0x04},   // ^
    {0x40, 0x40, 0x40, 0x40, 0x40},   // _
    {0x00, 0x03, 0x05, 0x00, 0x00},   // `
    {0x20, 0x54, 0x54, 0x54, 0x78},   // a
    {0x7F, 0x48, 0x44, 0x44, 0x38},   // b
    {0x38, 0x44, 0x44, 0x44, 0x20},   // c
    {0x38, 0x44, 0x44, 0x48, 0x7F},   // d
    {0x38, 0x54, 0x54, 0x54, 0x18},   // e
    {0x08, 0x7E, 0x09, 0x01, 0x02},   // f
    {0x18, 0xA4, 0xA4, 0xA4, 0x7C},   // g
    {0x7F, 0x08, 0x04, 0x04, 0x78},   // h
    {0x00, 0x44, 0x7D, 0x40, 0x00},   // i
    {0x40, 0x80, 0x84, 0x7D, 0x00},   // j
    {0x7F, 0x10, 0x28, 0x44, 0x00},   // k
    {0x00, 0x41, 0x7F, 0x40, 0x00},   // l
    {0x7C, 0x04, 0x18, 0x04, 0x78},   // m
    {0x7C, 0x08, 0x04, 0x04, 0x78},   // n
    {0x38, 0x44, 0x44, 0x44, 0x38},   // o
    {0xFC, 0x24, 0x24, 0x24, 0x18},   // p
    {0x18, 0x24, 0x24, 0x18, 0xFC},   // q
    {0x7C, 0x08, 0x04, 0x04, 0x08},   // r
    {0x48, 0x54, 0x54, 0x54, 0x20},   // s
    {0x04, 0x3F, 0x44, 0x40, 0x20},   // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C},   // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C},   // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C},   // w
    {0x44, 0x28, 0x10, 0x28, 0x44},   // x
    {0x1C, 0xA0, 0xA0, 0xA0, 0x7C},   // y
    {0x44, 0x64, 0x54, 0x4C, 0x44},   // z
    {0x00, 0x10, 0x7C, 0x82, 0x00},   // {
    {0x00, 0x00, 0xFF, 0x00, 0x00},   // |
    {0x00, 0x82, 0x7C, 0x10, 0x00},   // }
    {0x00, 0x06, 0x09, 0x09, 0x06}    // ~ (Degrees)
};
static char smile_face[2][16] =
{
    {0x00, 0xE0, 0x18, 0x04, 0x22, 0x22, 0x21, 0x01, 0x01, 0x21, 0x22, 0x22, 0x04, 0x18, 0xE0, 0x00},
    {0x00, 0x01, 0x06, 0x08, 0x10, 0x13, 0x25, 0x29, 0x29, 0x25, 0x13, 0x10, 0x08, 0x06, 0x01, 0x00}
};
static char sad_face[2][16] =
{
    {0x00, 0xE0, 0x18, 0x04, 0x32, 0xF2, 0x31, 0x01, 0x01, 0x31, 0xF2, 0x32, 0x04, 0x18, 0xE0, 0x00},
    {0x00, 0x01, 0x06, 0x08, 0x10, 0x1F, 0x2E, 0x2A, 0x2A, 0x2E, 0x1F, 0x10, 0x08, 0x06, 0x01, 0x00}
};

static enum hrtimer_restart hrtimer_callback(struct hrtimer *timer)
{
    if (atomic_read(&door_status))
    {
        if (pwm_state)
        {
            gpio_direction_output(GPIO_SERVO,0);
            hrtimer_forward_now(&hrtimer,time_period - time_high_open);
        }
        else
        {
            gpio_direction_output(GPIO_SERVO,1);
            hrtimer_forward_now(&hrtimer,time_high_open);
        }
        pwm_state = !pwm_state;
    }
    else
    {
        if (pwm_state)
        {
            gpio_direction_output(GPIO_SERVO,0);
            hrtimer_forward_now(&hrtimer,time_period - time_high_close);
        }
        else
        {
            gpio_direction_output(GPIO_SERVO,1);
            hrtimer_forward_now(&hrtimer,time_high_close);
        }
        pwm_state = !pwm_state;
    }
    return HRTIMER_RESTART;
}

static int i2c_write (char *buf, int len)
{
    int ret = i2c_master_send(i2c_client_oled, buf, len);
    return ret;
}
static void SSD1306_Write(bool is_cmd, unsigned char data)
{
    int ret;
    unsigned char buf[2] = {0};
    
    if( is_cmd == true )
    {
        buf[0] = 0x00;
    }
    else
    {
        buf[0] = 0x40;
    }
    
    buf[1] = data;
    
    ret = i2c_write(buf, 2);
}
static void SSD1306_SetCursor( uint8_t lineNo, uint8_t cursorPos )
{
  if((lineNo <= SSD1306_MAX_LINE) && (cursorPos < SSD1306_MAX_SEG))
  {
    SSD1306_LineNum   = lineNo;
    SSD1306_CursorPos = cursorPos;
    SSD1306_Write(true, 0x21);
    SSD1306_Write(true, cursorPos);
    SSD1306_Write(true, SSD1306_MAX_SEG-1);
    SSD1306_Write(true, 0x22);
    SSD1306_Write(true, lineNo);
    SSD1306_Write(true, SSD1306_MAX_LINE);
  }
}
static void  SSD1306_GoToNextLine( void )
{
    SSD1306_LineNum++;
    SSD1306_LineNum = (SSD1306_LineNum & SSD1306_MAX_LINE);
    SSD1306_SetCursor(SSD1306_LineNum,0);
}
static void SSD1306_PrintChar(unsigned char c)
{
  uint8_t data_byte;
  uint8_t temp = 0;
  if( (( SSD1306_CursorPos + SSD1306_FontSize ) >= SSD1306_MAX_SEG ) ||
      ( c == '\n' )
  )
  {
    SSD1306_GoToNextLine();
  }
  if( c != '\n' )
  {
    c -= 0x20;
    do
    {
      data_byte= SSD1306_font[c][temp];
      SSD1306_Write(false, data_byte);
      SSD1306_CursorPos++;
      
      temp++;
      
    } while ( temp < SSD1306_FontSize);
    SSD1306_Write(false, 0x00);
    SSD1306_CursorPos++;
  }
}
static void SSD1306_String(unsigned char *str)
{
  while(*str)
  {
    SSD1306_PrintChar(*str++);
  }
}
static int SSD1306_init (void)
{
    msleep(100);
    
    SSD1306_Write(true, 0xAE); // Entire Display OFF
    SSD1306_Write(true, 0xD5); // Set Display Clock Divide Ratio and Oscillator Frequency
    SSD1306_Write(true, 0x80); // Default Setting for Display Clock Divide Ratio and Oscillator Frequency that is recommended
    SSD1306_Write(true, 0xA8); // Set Multiplex Ratio
    SSD1306_Write(true, 0x3F); // 64 COM lines
    SSD1306_Write(true, 0xD3); // Set display offset
    SSD1306_Write(true, 0x00); // 0 offset
    SSD1306_Write(true, 0x40); // Set first line as the start line of the display
    SSD1306_Write(true, 0x8D); // Charge pump
    SSD1306_Write(true, 0x14); // Enable charge dump during display on
    SSD1306_Write(true, 0x20); // Set memory addressing mode
    SSD1306_Write(true, 0x00); // Horizontal addressing mode
    SSD1306_Write(true, 0xA1); // Set segment remap with column address 127 mapped to segment 0
    SSD1306_Write(true, 0xC8); // Set com output scan direction, scan from com63 to com 0
    SSD1306_Write(true, 0xDA); // Set com pins hardware configuration
    SSD1306_Write(true, 0x12); // Alternative com pin configuration, disable com left/right remap
    SSD1306_Write(true, 0x81); // Set contrast control
    SSD1306_Write(true, 0x80); // Set Contrast to 128
    SSD1306_Write(true, 0xD9); // Set pre-charge period
    SSD1306_Write(true, 0xF1); // Phase 1 period of 15 DCLK, Phase 2 period of 1 DCLK
    SSD1306_Write(true, 0xDB); // Set Vcomh deselect level
    SSD1306_Write(true, 0x20); // Vcomh deselect level ~ 0.77 Vcc
    SSD1306_Write(true, 0xA4); // Entire display ON, resume to RAM content display
    SSD1306_Write(true, 0xA6); // Set Display in Normal Mode, 1 = ON, 0 = OFF
    SSD1306_Write(true, 0x2E); // Deactivate scroll
    SSD1306_Write(true, 0xAF);
    return 0;
}
static void SSD1306_Fill(unsigned char data)
{
    unsigned int total  = 128 * 8;
    unsigned int i      = 0;
    
    for(i = 0; i < total; i++)
    {
        SSD1306_Write(false, data);
    }
}
static void SSD1306_Clear(unsigned int page)
{
    char cmd[4] = {0x00, 0xB0 + page, 0x00, 0x10};
    char data[129];
    i2c_write(cmd,4);
    data[0] = 0x40;
    memset(data+1, 0x00, 128);
    i2c_write(data,129);
}
static int oled_probe(struct i2c_client *client, const struct i2c_device_id *dev_id)
{
    SSD1306_init();
    for(int i = 0; i < 8; i++)
    {
        SSD1306_Clear(i);
    }
    SSD1306_SetCursor(1,7);
    SSD1306_String("Enter your password");
    pr_info("OLED Probed!!!\n");
    return 0;
}
static void oled_remove(struct i2c_client *client)
{
    SSD1306_SetCursor(0,0);
    SSD1306_Fill(0x00);
    pr_info("OLED Removed!!!\n");
    return ;
}
static const struct i2c_device_id oled_id[] = {
    { SLAVE_DEVICE_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, oled_id);
static struct i2c_driver oled_driver = {
    .driver = {
        .name   = SLAVE_DEVICE_NAME,
        .owner  = THIS_MODULE,
    },
    .probe          = oled_probe,
    .remove         = oled_remove,
    .id_table       = oled_id,
};
static struct i2c_board_info oled_i2c_board_info = {
    I2C_BOARD_INFO(SLAVE_DEVICE_NAME, SSD1306_SLAVE_ADDR)
};
static void oled_work_func(struct work_struct *work)
{
    int r = atomic_read(&row);
    int cur_pos = atomic_read(&c_pos);
    int pos;
    pos = cur_pos*10 + 37;
    if (oled_status)
    {
        if (!head_sta)
        {
            if (oled_c_status == 0)
            {
                for(int i = 0; i < 8; i++)
                {
                    SSD1306_Clear(i);
                }
                SSD1306_SetCursor(1,10);
                SSD1306_String("Enter old password");
            }
            else
            {
                for(int i = 0; i < 8; i++)
                {
                    SSD1306_Clear(i);
                }
                SSD1306_SetCursor(1,10);
                SSD1306_String("Enter new password");
            }
            head_sta = 1;
        }
    }
    else
    {
        if (!head_sta)
        {
            for(int i = 0; i < 8; i++)
            {
                SSD1306_Clear(i);
            }
            SSD1306_SetCursor(1,7);
            SSD1306_String("Enter your password");
            head_sta = 1;
        }
    }
    if (!(r == 2 && cols == 3) && r < 4)
    {
        if (r == 1 && cols == 3)
        {
            atomic_set(&c_pos, 0);
            SSD1306_Clear(3);
        }
        else if (r == 0 && cols == 3)
        {
            if (cur_pos)
            {
                SSD1306_SetCursor(3,pos - 10);
                SSD1306_PrintChar(' ');
                atomic_dec(&c_pos);
            }
        }
        else
        {
            SSD1306_SetCursor(3,pos);
            SSD1306_PrintChar(pass[cur_pos]);
            msleep(100);
            SSD1306_SetCursor(3,pos);
            SSD1306_PrintChar(' ');
            msleep(100);
            SSD1306_SetCursor(3,pos);
            SSD1306_PrintChar('*');
            atomic_inc(&c_pos);
            cur_pos++;
        }
        if (cur_pos == 6)
        {
            if (oled_status == 0)
            {
                for(int i = 0; i < 8; i++)
                {
                    SSD1306_Clear(i);
                }
                msleep(500);
                if(memcmp(pass, c_pass, 6) == 0)
                {
                    for(int i = 0; i < 2; i++)
                    {
                        SSD1306_SetCursor(i+1,56);
                        for (int j = 0; j < 16; j++)
                        {
                            SSD1306_Write(false, smile_face[i][j]);
                        }
                    }
                    SSD1306_SetCursor(4,22);
                    SSD1306_String("DOOR WILL OPEN");
                    now = jiffies;
                    atomic_set(&door_status, 1);
                }
                else
                {
                    for(int i = 0; i < 2; i++)
                    {
                        SSD1306_SetCursor(i+1,56);
                        for (int j = 0; j < 16; j++)
                        {
                            SSD1306_Write(false, sad_face[i][j]);
                        }
                    }
                    SSD1306_SetCursor(4,10);
                    SSD1306_String("INCORRECT PASSWORD");
                }
                msleep(500);
                atomic_set(&c_pos, 0);
                atomic_set(&row, 4);
                cols = 4;
                head_sta = 0;
                queue_work(oled_wq,&oled_work);
            }
            else
            {
                for(int i = 0; i < 8; i++)
                {
                    SSD1306_Clear(i);
                }
                msleep(500);
                if (oled_c_status == 0)
                {
                    if(memcmp(pass, c_pass, 6) == 0)
                    {
                        SSD1306_SetCursor(4,16);
                        SSD1306_String("CORRECT PASSWORD");
                        oled_c_status = 1;
                    }
                    else
                    {
                        SSD1306_SetCursor(4,10);
                        SSD1306_String("INCORRECT PASSWORD");
                    }
                    msleep(1000);
                    atomic_set(&c_pos, 0);
                    atomic_set(&row, 4);
                    cols = 4;
                    head_sta = 0;
                    queue_work(oled_wq,&oled_work);
                }
                else
                {
                    memcpy(c_pass, pass, sizeof(pass));
                    SSD1306_SetCursor(3,16);
                    SSD1306_String("PASSWORD UPDATED");
                    msleep(1000);
                    oled_status = !oled_status;
                    atomic_set(&c_pos, 0);
                    atomic_set(&row, 4);
                    cols = 4;
                    head_sta = 0;
                    queue_work(oled_wq,&oled_work);
                }
            }
        }
    }
}

static void my_work_func(struct work_struct *work)
{
    for (int i = 4; i < 8; i++)
    {
        gpio_set_value(gpios[i], 0);
        atomic_set(&row, i - 4);
        msleep(50);
        gpio_set_value(gpios[i], 1);
    }
    queue_delayed_work(my_wq, &my_work, msecs_to_jiffies(300));
}

static irqreturn_t button_irq(int irq, void *dev_id)
{
    int *p = (int *)dev_id;
    int r = atomic_read(&row);
    int cur_pos = atomic_read(&c_pos);
    cols = -1;
    for (int i = 0; i < 4; i++)
    {
        if(*p == gpios[i])
        {
            cols = i;
            break;
        }
    }
    if (cols == -1) return IRQ_HANDLED;
    
    if (r == 3)
    {
        if (cols == 1)
        {
            pass[cur_pos] = matrix[r][cols];
            queue_work(oled_wq,&oled_work);
        }
    }
    else if (r == 2)
    {
        if (cols != 3)
        {
            pass[cur_pos] = matrix[r][cols];
            queue_work(oled_wq,&oled_work);
        }
        else
        {
            oled_status = !oled_status;
            oled_c_status = 0;
            atomic_set(&c_pos, 0);
            head_sta = 0;
            queue_work(oled_wq,&oled_work);
        }
    }
    else if (r == 1)
    {
        if (cols != 3)
        {
            pass[cur_pos] = matrix[r][cols];
            queue_work(oled_wq,&oled_work);
        }
        else
        {
            queue_work(oled_wq,&oled_work);
        }
    }
    else
    {
        if (cols != 3)
        {
            pass[cur_pos] = matrix[r][cols];
            queue_work(oled_wq,&oled_work);
        }
        else
        {
            queue_work(oled_wq,&oled_work);
        }
    }
    
    return IRQ_HANDLED;
}

static irqreturn_t irq_handle(int irq, void *dev_id)
{
    int cur = atomic_read(&door_status);
    now = jiffies;
    if (time_before(now, last_jiffies + msecs_to_jiffies(500)))
        return IRQ_HANDLED;
    last_jiffies = now;
    atomic_set(&door_status, !cur);
    return IRQ_HANDLED;
}

int gpios_init(void)
{
    int ret;
    for (int i = 0; i < 8; i++)
    {
        if (i > 3)
        {
            if(gpio_is_valid(gpios[i]) == false)
            {
                pr_err("ERROR: GPIO %d invalid\n", gpios[i]);
                return 0;
            }
            if(gpio_request(gpios[i],"ROW_GPIO") < 0)
            {
                pr_err("ERROR: GPIO %d request\n", gpios[i]);
                return 0;
            }
            gpio_direction_output(gpios[i],1);
        }
        else
        {
            if(gpio_is_valid(gpios[i]) == false)
            {
                pr_err("GPIO %d is not valid\n", gpios[i]);
                return 0;
            }
            if(gpio_request(gpios[i],"COLS_GPIO") < 0)
            {
                pr_err("ERROR: GPIO %d request\n", gpios[i]);
                return 0;
            }
            gpio_direction_input(gpios[i]);
            irq_nums[i] = gpio_to_irq(gpios[i]);
            if (irq_nums[i] < 0)
            {
                return 0;
            }
            ret = request_irq(irq_nums[i], (irq_handler_t) button_irq, IRQF_TRIGGER_FALLING, "tabi_interrupt", (gpios+i));
            if(ret)
            {
                printk("failed interrupt\n");
                return 0;
            }
        }
    }
    if(gpio_is_valid(GPIO_SERVO) == false)
    {
        pr_err("ERROR: GPIO %d invalid\n",GPIO_SERVO);
        return 0;
    }
    if(gpio_request(GPIO_SERVO,"GPIO") < 0)
    {
        pr_err("ERROR: GPIO %d request\n", GPIO_SERVO);
        return 0;
    }
    gpio_direction_output(GPIO_SERVO,0);
    if(gpio_is_valid(GPIO_BUTTON) == false)
    {
        pr_err("GPIO %d is not valid\n", GPIO_BUTTON);
        return 0;
    }
    if(gpio_request(GPIO_BUTTON,"COLS_GPIO") < 0)
    {
        pr_err("ERROR: GPIO %d request\n", GPIO_BUTTON);
        return 0;
    }
    gpio_direction_input(GPIO_BUTTON);
    irq_nums[4] = gpio_to_irq(GPIO_BUTTON);
    if (irq_nums[4] < 0)
    {
        return 0;
    }
    ret = request_irq(irq_nums[4], (irq_handler_t) irq_handle, IRQF_TRIGGER_RISING, "button interrupt", NULL);
    if(ret)
    {
        printk("failed interrupt\n");
        return 0;
    }
    return 1;
}

static int __init tabi_driver_init(void)
{
    int ret;
    if((alloc_chrdev_region(&dev, 0, 1, "tabi_dev")) <0)
    {
        pr_err("Cannot allocate major number\n");
        return -1;
    }
    pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));
    cdev_init(&tabi_cdev,&fops);
    if((cdev_add(&tabi_cdev,dev,1)) < 0)
    {
        pr_err("Cannot add the device to the system\n");
        goto r_class;
    }
    if(IS_ERR(tabi_class = class_create(THIS_MODULE,"tabi_class")))
    {
        pr_err("Cannot create the struct class\n");
        goto r_class;
    }
    if(IS_ERR(device_create(tabi_class,NULL,dev,NULL,"tabi_device")))
    {
        pr_err("Cannot create the Device \n");
        goto r_device;
    }
    hrtimer_init(&hrtimer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hrtimer.function = &hrtimer_callback;
    time_high_open = ktime_set(0,TIME_HIGH_OPEN);
    time_high_close = ktime_set(0,TIME_HIGH_CLOSE);
    time_period = ktime_set(0,TIME_PERIOD);
    oled_wq = create_workqueue("oled_wq");
    if (!oled_wq)
        goto r2;
    INIT_WORK(&oled_work, oled_work_func);
    my_wq = create_workqueue("my_wq");
    if (!my_wq)
        goto r1;
    INIT_DELAYED_WORK(&my_work, my_work_func);
    i2c_adapter     = i2c_get_adapter(I2C_BUS_AVAILABLE);
    if( i2c_adapter != NULL )
    {
        i2c_client_oled = i2c_new_client_device(i2c_adapter, &oled_i2c_board_info);
        if(i2c_client_oled != NULL )
        {
            i2c_add_driver(&oled_driver);
        }
        i2c_put_adapter(i2c_adapter);
    }
    ret = gpios_init();
    if (ret != 1)
        goto r;
    hrtimer_start(&hrtimer, time_high_close, HRTIMER_MODE_REL);
    queue_delayed_work(my_wq, &my_work,msecs_to_jiffies(500));
    pr_info("Module loaded.\n");
    return 0;
r:
    cancel_delayed_work_sync(&my_work);
    flush_workqueue(my_wq);
    destroy_workqueue(my_wq);
    i2c_unregister_device(i2c_client_oled);
    i2c_del_driver(&oled_driver);
r1:
    cancel_work_sync(&oled_work);
    flush_workqueue(oled_wq);
    destroy_workqueue(oled_wq);
r2:
    hrtimer_cancel(&hrtimer);
    gpio_free(GPIO_SERVO);
r_device:
    class_destroy(tabi_class);
    cdev_del(&tabi_cdev);
r_class:
    unregister_chrdev_region(dev,1);
    return -1;
}

static void __exit tabi_driver_exit(void)
{
    hrtimer_cancel(&hrtimer);
    i2c_unregister_device(i2c_client_oled);
    i2c_del_driver(&oled_driver);
    cancel_work_sync(&oled_work);
    flush_workqueue(oled_wq);
    destroy_workqueue(oled_wq);
    cancel_delayed_work_sync(&my_work);
    flush_workqueue(my_wq);
    destroy_workqueue(my_wq);
    gpio_free(GPIO_SERVO);
    gpio_free(GPIO_BUTTON);
    free_irq(irq_nums[4], NULL);
    for (int i = 0; i < 4; i++)
    {
        free_irq(irq_nums[i], (gpios+i));
    }
    for(int i = 0; i < 8; i++)
    {
        gpio_free(gpios[i]);
    }
    device_destroy(tabi_class,dev);
    class_destroy(tabi_class);
    cdev_del(&tabi_cdev);
    unregister_chrdev_region(dev, 1);
    pr_info("Module unloaded.\n");
}

module_init(tabi_driver_init);
module_exit(tabi_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("hungtabi1478@gmail.com");
MODULE_DESCRIPTION("on of led");
MODULE_VERSION("1.32");
