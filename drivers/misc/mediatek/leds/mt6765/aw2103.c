
#include <linux/hct_include/hct_project_all_config.h>
#ifndef __HCT_BREATH_LIGHT_SUPPORT__
#define __HCT_BREATH_LIGHT_SUPPORT__	HCT_NO
#endif

#if __HCT_BREATH_LIGHT_SUPPORT__
#include "aw2103.h"
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/fs.h>

#include <linux/wakelock.h>

#define TS_DEBUG_MSG 			0

//////////////////////////////////////
// IO PIN DEFINE
//////////////////////////////////////
#ifdef GPIO_AW9136_PDN_PIN
#define AW2103_PDN          GPIO_AW9136_PDN_PIN
#endif

#define R_LED_CHN    0X4
#define G_LED_CHN    0X2
#define B_LED_CHN    0X1

#define IIC_ADDRESS_WRITE			0x8a             //I2C 写地址
#define IIC_ADDRESS_READ			0x8b             //I2C 读地址
#define AW2013_I2C_MAX_LOOP 		5   
#define I2C_delay 		2    //可根据平台调整,保证I2C速度不高于400k

//以下为调节呼吸效果的参数
#ifdef __HCT_AW2013_BREATHLIGHT_IMAX__
#define Imax   __HCT_AW2013_BREATHLIGHT_IMAX__   
#else
#define Imax        0x02   //LED最大电流配置,0x00=omA,0x01=5mA,0x02=10mA,0x03=15mA,
#endif
#define Rise_time   0x02   //LED呼吸上升时间,0x00=0.13s,0x01=0.26s,0x02=0.52s,0x03=1.04s,0x04=2.08s,0x05=4.16s,0x06=8.32s,0x07=16.64s
#define Hold_time   0x01   //LED呼吸到最亮时的保持时间0x00=0.13s,0x01=0.26s,0x02=0.52s,0x03=1.04s,0x04=2.08s,0x05=4.16s
#define Fall_time     0x02   //LED呼吸下降时间,0x00=0.13s,0x01=0.26s,0x02=0.52s,0x03=1.04s,0x04=2.08s,0x05=4.16s,0x06=8.32s,0x07=16.64s
#define Off_time      0x01   //LED呼吸到灭时的保持时间0x00=0.13s,0x01=0.26s,0x02=0.52s,0x03=1.04s,0x04=2.08s,0x05=4.16s,0x06=8.32s,0x07=16.64s
#define Delay_time   0x00   //LED呼吸启动后的延迟时间0x00=0s,0x01=0.13s,0x02=0.26s,0x03=0.52s,0x04=1.04s,0x05=2.08s,0x06=4.16s,0x07=8.32s,0x08=16.64s
#define Period_Num  0x00   //LED呼吸次数0x00=无限次,0x01=1次,0x02=2次.....0x0f=15次

/*
static ssize_t aw2013_store_led(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t aw2013_get_reg(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t aw2013_set_reg(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);

static DEVICE_ATTR(led, S_IRUGO | S_IWUSR, NULL, aw2013_store_led);
static DEVICE_ATTR(reg, S_IRUGO | S_IWUGO,aw2013_get_reg,  aw2013_set_reg);
*/

//Begin-Bee-20140417
struct i2c_client *aw2013_i2c_client;
//End-Bee-20140417


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if 0
static ssize_t AW2013_get_reg(struct device* cd,struct device_attribute *attr, char* buf);
static ssize_t AW2013_set_reg(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t AW2013_set_debug(struct device* cd, struct device_attribute *attr,const char* buf, size_t len);
static ssize_t AW2013_get_debug(struct device* cd, struct device_attribute *attr, char* buf);

static DEVICE_ATTR(reg, 0660, AW2013_get_reg,  AW2013_set_reg);
static DEVICE_ATTR(debug, 0660, AW2013_get_debug,  AW2013_set_debug);
#endif

void AW2013_delay_1us(unsigned long wTime);   //
int AW2013_i2c_write_reg(unsigned char reg,unsigned char data);

#define AW_TIME_VAL_SIZE   8
unsigned int timems_trans_arry[AW_TIME_VAL_SIZE]= {130, 260, 520,1040, 2080, 4160, 8320, 16640};

unsigned char aw2103_convert_time2val(unsigned int time)
{
       unsigned int time_val = 0;
       unsigned char check_time_val = 0;
       if(time<timems_trans_arry[0])
          check_time_val =0;
       else if(time>timems_trans_arry[AW_TIME_VAL_SIZE-1])
          check_time_val =AW_TIME_VAL_SIZE-1;
       else
       {
           for(time_val=1; time_val<= AW_TIME_VAL_SIZE-1;time_val++)
           {
                if(time >timems_trans_arry[time_val-1]&& time <=timems_trans_arry[time_val] )
                {
                    if ( (time +timems_trans_arry[time_val-1])> timems_trans_arry[time_val])
                    {
                        check_time_val = time_val;
                    }
                    else
                        check_time_val = time_val -1;
                }
           }
       }
       return check_time_val;
}

//*******************************AW2013呼吸灯程序***********************************///


// on/off time max is 16.384s
// rasing/fainling time max is 4096ms
#define bright_div_val   128
#define HCT_PROJ_MAX_SUPPORT_LIGHT   3

unsigned char AW2013_i2c_read_reg(unsigned char regaddr);
#if 0
static void aw2103_holdlight_by_chip(unsigned int leds)
{
		if(leds > 7)
	    {
	       printk("leds is wrige setting set =%d, need less 7\n", leds);
	    }


		AW2013_i2c_write_reg(0x00, 0x55);				// Reset ==pdn
		AW2013_i2c_write_reg(0x01, 0x01);		// enable LED 不使用中断

	switch(leds)
		{
			case G_LED_CHN:
				AW2013_i2c_write_reg(0x31, Imax|0x00);	//config mode, IMAX = 5mA
				AW2013_i2c_write_reg(0x34, 0xff);	// LED0 level,			
			break;

			case B_LED_CHN:
				AW2013_i2c_write_reg(0x32, Imax|0x00);	//config mode, IMAX = 5mA
				AW2013_i2c_write_reg(0x35, 0xff);	// LED1 level,		
			break;

			case R_LED_CHN:
				AW2013_i2c_write_reg(0x33, Imax|0x00);	//config mode, IMAX = 5mA
				AW2013_i2c_write_reg(0x36, 0xff);	// LED2 level,
			break;

			case R_LED_CHN|G_LED_CHN|B_LED_CHN:
				AW2013_i2c_write_reg(0x31, Imax|0x00);	//config mode, IMAX = 5mA
				AW2013_i2c_write_reg(0x32, Imax|0x00);	//config mode, IMAX = 5mA
				AW2013_i2c_write_reg(0x33, Imax|0x00);	//config mode, IMAX = 5mA
				AW2013_i2c_write_reg(0x34, 0xff);	// LED0 level,
				AW2013_i2c_write_reg(0x35, 0xff);	// LED1 level,
				AW2013_i2c_write_reg(0x36, 0xff);	// LED2 level,
			break;

			default:
			break;
		}
        AW2013_i2c_write_reg(0x30,leds);
 		printk("----hct_leds_holdlight_by_chip-----ok-----\n");
  

}
#else
static void aw2103_holdlight_by_chip(unsigned int leds)
{
	unsigned char reg_old = 0; 
	int i =0;

	pr_err("%s: hct-drv. leds = %d. \n", __func__, leds);

	if(leds > 7)
	    {
	       printk("leds is wrige setting set =%d, need less 7\n", leds);
	    }
           
           else
            {
		reg_old = AW2013_i2c_read_reg(0x30);
               // AW2013_i2c_write_reg(0x00, 0x55);				// Reset ==pdn
		AW2013_i2c_write_reg(0x01, 0x01);		// enable LED 不使用中断

		//AW2013_i2c_write_reg(0x31, Imax|0x00);	//config mode, IMAX = 5mA
		//AW2013_i2c_write_reg(0x32, Imax|0x00);	//config mode, IMAX = 5mA
		//AW2013_i2c_write_reg(0x33, Imax|0x00);	//config mode, IMAX = 5mA
		AW2013_i2c_write_reg(0x34, 0xff);	// LED0 level,
		AW2013_i2c_write_reg(0x35, 0xff);	// LED1 level,
		AW2013_i2c_write_reg(0x36, 0xff);	// LED2 level,		

		for(i=0;i<HCT_PROJ_MAX_SUPPORT_LIGHT;i++)
		{
		if((0x01<<i)&leds){  // i means i channel is enable
		AW2013_i2c_write_reg(0x31 + i, Imax|0x00);
		}
		}

	
                AW2013_i2c_write_reg(0x30,(0x07&leds));
		
 		printk("----hct_leds_holdlight_by_chip-----ok----- reg_old = %d; leds = %d\n", reg_old, leds);
                }
}
#if 0
void AW2013_LED_ON(unsigned int RED,unsigned int GREEN,unsigned int BLUE)
{

	AW2013_i2c_write_reg(0x00, 0x55);				// Reset ==pdn
	AW2013_i2c_write_reg(0x01, 0x01);		// enable LED 不使用中断

	AW2013_i2c_write_reg(0x31, Imax|0x00);	//config mode, IMAX = 5mA
	AW2013_i2c_write_reg(0x32, Imax|0x00);	//config mode, IMAX = 5mA
	AW2013_i2c_write_reg(0x33, Imax|0x00);	//config mode, IMAX = 5mA
	AW2013_i2c_write_reg(0x34, RED);	// LED0 level,
	AW2013_i2c_write_reg(0x35, GREEN);	// LED1 level,
	AW2013_i2c_write_reg(0x36, BLUE);	// LED2 level,			
	AW2013_i2c_write_reg(0x30,0x07);

	printk("----hct_leds_AW2013_LED_ON_ RED = %d; GREEN = %d; BLUE= %d \n", RED,GREEN,BLUE);

}
#endif
#endif
static void AW2013_LED_Blink(unsigned int leds,int rasing_time, int on_time,int failing_time,int off_time, unsigned int cycle_value)//r-g-b, trise, ton, tfall, tfall , cycle
{
    	unsigned int rasing_val=0, on_val=0,failig_val=0,off_val=0;
	unsigned char reg_old = 0;
	

    	int i=0;
	reg_old = AW2013_i2c_read_reg(0x30);

	rasing_val = aw2103_convert_time2val(rasing_time);
	on_val = aw2103_convert_time2val(on_time);
	failig_val = aw2103_convert_time2val(failing_time);
	off_val = aw2103_convert_time2val(off_time);

	if(on_val>5)
	on_val = 5;


	if(cycle_value>15)
	cycle_value=0;  // is cycle value over 10, we set to infinite

	if(leds > 7)
	{
	printk("leds is wrige setting set =%d, need less 7\n", leds);
	}

//	AW2013_i2c_write_reg(0x00, 0x55);				// Reset ==pdn
	AW2013_i2c_write_reg(0x01, 0x01);		// enable LED ²»Ê¹ÓÃÖÐ¶Ï		
	AW2013_i2c_write_reg(0x30, 0x00);
	//AW2013_i2c_write_reg(0x31, Imax|0x70);	//config mode, IMAX = 5mA	
	//AW2013_i2c_write_reg(0x32, Imax|0x70);	//config mode, IMAX = 5mA	
	//AW2013_i2c_write_reg(0x33, Imax|0x70);	//config mode, IMAX = 5mA	

	AW2013_i2c_write_reg(0x34, 0xff);	// LED0 level,
	AW2013_i2c_write_reg(0x35, 0xff);	// LED1 level,
	AW2013_i2c_write_reg(0x36, 0xff);	// LED2 level,
	/*
	AW2013_i2c_write_reg(0x04,0x03);
	AW2013_i2c_write_reg(0x05,0x0F);
	AW2013_i2c_write_reg(0x15,(failig_val<<3)|rasing_val);
	AW2013_i2c_write_reg(0x16,(off_val<<3)|on_val);
	*/

	for(i=0;i<HCT_PROJ_MAX_SUPPORT_LIGHT;i++)
	{
	if((0x01<<i)&leds){  // i means i channel is enable
	AW2013_i2c_write_reg(0x31 + i, Imax|0x70);	//config mode, IMAX = 5mA
	AW2013_i2c_write_reg(0x37 +(i*3), rasing_val<<4 | on_val);   //led0  ÉÏÉýÊ±Œä£¬±£³ÖÊ±ŒäÉè¶š                          
	AW2013_i2c_write_reg(0x38+(i*3), failig_val<<4 | off_val);           //led0 ÏÂœµÊ±Œä£¬¹Ø±ÕÊ±ŒäÉè¶š
	AW2013_i2c_write_reg(0x39+(i*3), Delay_time<<4| cycle_value);   //led0  ºôÎüÑÓ³ÙÊ±Œä£¬ºôÎüÖÜÆÚÉè¶š
	}
	}


	printk("----hct_leds_breath_by_chip, ras=%d,on=%d,fail=%d,off=%d-------\n",rasing_val,on_val,failig_val,off_val);

	AW2013_i2c_write_reg(0x30,(0x07&leds)|reg_old);

	AW2013_delay_1us(8);//ÐèÑÓÊ±5usÒÔÉÏ

}
static void aw2103_breath_by_chip(unsigned int leds,int rasing_time, int on_time,int failing_time,int off_time, unsigned int cycle_value)
{
    unsigned int rasing_val=0, on_val=0,failig_val=0,off_val=0;
    int i=0;

    rasing_val = aw2103_convert_time2val(rasing_time);
    on_val = aw2103_convert_time2val(on_time);
    failig_val = aw2103_convert_time2val(failing_time);
    off_val = aw2103_convert_time2val(off_time);


    if(on_val>5)
        on_val = 5;
 

    if(cycle_value>15)
        cycle_value=0;  // is cycle value over 10, we set to infinite

    if(leds > 7)
    {
       printk("leds is wrige setting set =%d, need less 7\n", leds);
    }
 
   AW2013_i2c_write_reg(0x00, 0x55);				// Reset ==pdn
   AW2013_i2c_write_reg(0x01, 0x01);		// enable LED 不使用中断		
   
   AW2013_i2c_write_reg(0x31, Imax|0x70);	//config mode, IMAX = 5mA	
   AW2013_i2c_write_reg(0x32, Imax|0x70);	//config mode, IMAX = 5mA	
   AW2013_i2c_write_reg(0x33, Imax|0x70);	//config mode, IMAX = 5mA	
   
   AW2013_i2c_write_reg(0x34, 0xff);	// LED0 level,
   AW2013_i2c_write_reg(0x35, 0xff);	// LED1 level,
   AW2013_i2c_write_reg(0x36, 0xff);	// LED2 level,
/*
    AW2013_i2c_write_reg(0x04,0x03);
    AW2013_i2c_write_reg(0x05,0x0F);
    AW2013_i2c_write_reg(0x15,(failig_val<<3)|rasing_val);
    AW2013_i2c_write_reg(0x16,(off_val<<3)|on_val);
*/
    
    for(i=0;i<HCT_PROJ_MAX_SUPPORT_LIGHT;i++)
    {
    	if((0x01<<i)&leds){  // i means i channel is enable
            AW2013_i2c_write_reg(0x37 +(i*3), rasing_val<<4 | on_val);   //led0  上升时间，保持时间设定                          
            AW2013_i2c_write_reg(0x38+(i*3), failig_val<<4 | off_val);           //led0 下降时间，关闭时间设定
            AW2013_i2c_write_reg(0x39+(i*3), Delay_time<<4| cycle_value);   //led0  呼吸延迟时间，呼吸周期设定
            
            
           }
    }

    printk("----hct_leds_breath_by_chip, ras=%d,on=%d,fail=%d,off=%d-------\n",rasing_val,on_val,failig_val,off_val);

    AW2013_i2c_write_reg(0x30,leds);
    
	AW2013_delay_1us(8);//需延时5us以上

}


void led_off_aw2013(void)//( unsigned int id )
{
//	unsigned char reg_data;
//	unsigned int	reg_buffer[8];

    AW2013_i2c_write_reg(0x30, 0);				//led off	
    AW2013_i2c_write_reg(0x01,0);
    AW2013_i2c_write_reg(0x00, 0x55);                // Reset ==pdn

}
#if 0
void led_off_aw2013_choose(int led)//( unsigned int id )
{
	unsigned char reg_data;
	reg_data = AW2013_i2c_read_reg(0x30);
	if(0 == reg_data){
		printk("HCT__All leds is already close__\n");
	}else{
		switch(led){
			case 0x01:
			reg_data = 0x0e & reg_data;		
    			AW2013_i2c_write_reg(0x30, reg_data);	
			break;
			case 0x02:
			reg_data = 0x0d & reg_data;
			AW2013_i2c_write_reg(0x30, reg_data);
			break;
			case 0x03:
			AW2013_i2c_write_reg(0x30, 0);
			break;
			default:
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Debug
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t AW2013_get_reg(struct device* cd,struct device_attribute *attr, char* buf)
{
	unsigned char reg_val;
	ssize_t len = 0;
	u8 i;
	for(i=0;i<0x3F;i++)
	{
		reg_val = AW2013_i2c_read_reg(i);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg%2X = 0x%2X, ", i,reg_val);
	}

	return len;
}

static ssize_t AW2013_set_reg(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned int databuf[2];
	if(2 == sscanf(buf,"%x %x",&databuf[0], &databuf[1]))
	{
		AW2013_i2c_write_reg(databuf[0],databuf[1]);
	}
	return len;
}


static ssize_t AW2013_get_debug(struct device* cd,struct device_attribute *attr, char* buf)
{
	ssize_t len = 0;
	len += snprintf(buf+len, PAGE_SIZE-len, "AW2013_LED_OFF(void)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 0 > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "AW2013_LED_ON(r, g, b)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 1  r   g   b  > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 1 255 255 255 > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "AW2013_LED_Blink(r-g-b, trise, ton, tfall, tfall,cycle)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 2 r-g-b  trise ton tfall toff cycle > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 2    7   1000    0 1000  1000     0 > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "led_off_aw2013_choose(led)\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 3 r-g-b  > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "echo 3 3     > debug\n");
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	return len;
}
static ssize_t AW2013_set_debug(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
	unsigned int databuf[16];
	sscanf(buf,"%d",&databuf[0]);
	if(databuf[0] == 0) {		// OFF
		led_off_aw2013(); //ALL - OFF	
	} else if(databuf[0] == 1) {	//ON
		sscanf(&buf[1], "%d %d %d", &databuf[1], &databuf[2], &databuf[3]);
		//aw2103_holdlight_by_chip(databuf[1], databuf[2], databuf[3]);
		AW2013_LED_ON(databuf[1], databuf[2], databuf[3]);
	} else if(databuf[0] == 2) {	//Blink
		sscanf(&buf[1], "%d %d %d %d %d %d", &databuf[1], &databuf[2], &databuf[3], &databuf[4], &databuf[5], &databuf[6]);
		AW2013_LED_Blink(databuf[1], databuf[2], databuf[3], databuf[4], databuf[5], databuf[6]);
	}else if(databuf[0] == 3) {	//Blink
		sscanf(&buf[1], "%d", &databuf[1]);
		led_off_aw2013_choose(databuf[1]);
	}
	return len;
}



static int AW2013_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	err = device_create_file(dev, &dev_attr_reg);
	err = device_create_file(dev, &dev_attr_debug);
	return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif
void led_off_aw2013_mode(int mode)//( unsigned int id )
{
	unsigned char reg_data;
	reg_data = AW2013_i2c_read_reg(0x30);
	printk("__hct__: reg_data = %d, modem = %d \n", reg_data, mode);
	if(0 == reg_data){
		printk("HCT__All leds is already close__\n");
              //  AW2013_i2c_write_reg(0x30, 0);
              //  printk("__hct__:close all.  reg_data = %d, modem = %d \n", reg_data, mode);
	}else{
		//AW2013_i2c_write_reg(0x30, 0x01);
//	unsigned int	reg_buffer[8];

		switch(mode){
case 0:
			case 5: //blue
			reg_data = 0xf8 & reg_data;		
    			AW2013_i2c_write_reg(0x30, reg_data);				//led off	
 //   			AW2013_i2c_write_reg(0x01,0);
  //  			AW2013_i2c_write_reg(0x00, 0x55);                // Reset ==pdn
			printk("__hct__:red__ reg_data = %d, modem = %d \n", reg_data, mode);
			break;
			case 8: //green
			reg_data = 0x0d & reg_data;
			AW2013_i2c_write_reg(0x30, reg_data);
			printk("__hct__:green__ reg_data = %d, modem = %d \n", reg_data, mode);
			break;
			case 6:
			AW2013_i2c_write_reg(0x30, 0);
			break;
			default:
			break;
		}
	}
}


void AW2013_delay_1us(unsigned long wTime)   //
{
	udelay(wTime);
}

// Bee-20140417
int AW2013_i2c_write_reg(unsigned char reg,unsigned char data)
{
	int ret=0;
	unsigned char i;

	unsigned char wrbuf[2];
	wrbuf[0] = reg;
	wrbuf[1] = data;

	pr_err("%s : 1\n", __func__);
	if(aw2013_i2c_client == NULL)
		pr_err("%s : aw2013_i2c_client == NULL\n", __func__);

	for (i=0; i<AW2013_I2C_MAX_LOOP; i++) {
		ret = i2c_master_send(aw2013_i2c_client, wrbuf, 2);
		pr_err("%s : 2=%d\n", __func__, ret);
		if (ret == 2) {
			break;
		}
	}

	return ret;
}


unsigned char AW2013_i2c_read_reg(unsigned char regaddr) 
{
	unsigned char rdbuf[1], wrbuf[1], ret, i;

	wrbuf[0] = regaddr;

	for (i=0; i<AW2013_I2C_MAX_LOOP; i++) {
		ret = i2c_master_send(aw2013_i2c_client, wrbuf, 1);
		if (ret == 1)
			break;
	}
	ret = i2c_master_recv(aw2013_i2c_client, rdbuf, 1);
	if (ret != 1)
		dev_err(&aw2013_i2c_client->dev,
			"%s: i2c_master_recv() failed, ret=%d\n",
			__func__, ret);

    	return rdbuf[0];
}
//#endif //aw2013

extern struct i2c_adapter * get_mt_i2c_adaptor(int);
int breathlight_master_send(u16 addr, char * buf ,int count)
{
	unsigned char ret;
	ret = i2c_master_send(aw2013_i2c_client, buf, count);
	if (ret != count) {
		dev_err(&aw2013_i2c_client->dev,
			"%s: i2c_master_recv() failed, ret=%d\n",
			__func__, ret);
	}
	return ret;
}

/////////////////////////////for aw2013




/*
//begin-Bee-20140417
static int aw2013_create_sysfs(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	TS_DBG("%s", __func__);
	
	err = device_create_file(dev, &dev_attr_led);
	err = device_create_file(dev, &dev_attr_reg);
	return err;
}
*/
#ifdef AW2103_PDN
static void aw2013_ts_pwron(void)
{
    mt_set_gpio_mode(AW2103_PDN,GPIO_MODE_00);  
    mt_set_gpio_dir(AW2103_PDN,GPIO_DIR_OUT);
    mt_set_gpio_out(AW2103_PDN,GPIO_OUT_ONE);
    msleep(20);
}

static void aw2013_ts_pwroff(void)
{
	mt_set_gpio_out(AW2103_PDN,GPIO_OUT_ZERO);  
}

static void aw2013_ts_config_pins(void)
{
	aw2013_ts_pwron();   //jed
	msleep(10); //wait for stable
}
#endif

struct pinctrl *aw2013_pinsctrl = NULL;
struct pinctrl_state *aw2013_default = NULL;
struct pinctrl_state *aw2013_ldo_low = NULL;
struct pinctrl_state *aw2013_ldo_high = NULL;

int aw2013_gpio_init(struct platform_device *pdev)
{
	int ret = 0;
	printk("aw2013_gpio_init start1111111111\n");
	aw2013_pinsctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(aw2013_pinsctrl)) {
		dev_err(&pdev->dev, "Cannot find aw2013 pinctrl!");
		ret = PTR_ERR(aw2013_pinsctrl);
	}

	/*aw2013 ldo Pin initialization */
	aw2013_ldo_low = pinctrl_lookup_state(aw2013_pinsctrl, "state_aw2013_low");
	if (IS_ERR(aw2013_ldo_low)) {
		ret = PTR_ERR(aw2013_ldo_low);
		pr_err("%s : pinctrl err, aw2013_ldo_low\n", __func__);
	}

	/*aw2013 ldo Pin initialization */
	aw2013_ldo_high = pinctrl_lookup_state(aw2013_pinsctrl, "state_aw2013_high");
	if (IS_ERR(aw2013_ldo_high)) {
		ret = PTR_ERR(aw2013_ldo_high);
		pr_err("%s : pinctrl err, aw2013_ldo_high\n", __func__);
	}
	printk("aw2013_gpio_init end1111111111\n");
	return ret;
}

static void aw2013_ldo_gpio_output(int level)
{
	if ((IS_ERR(aw2013_ldo_low))||(IS_ERR(aw2013_ldo_high))) {
		pr_err("%s : pinctrl err, aw2013_ldo_low || aw2013_ldo_high\n", __func__);
	}
	if (level)
		pinctrl_select_state(aw2013_pinsctrl, aw2013_ldo_high);
	else
		pinctrl_select_state(aw2013_pinsctrl, aw2013_ldo_low);
}
void ISSI_led_function(int onoff,int mode,int led_brightness);
static int aw2013_ready = 0;
static int aw2013_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
#ifdef AW2103_PDN
      aw2013_ts_config_pins();
#endif

	pr_err("aw2013_i2c_probe start\n");
	aw2013_i2c_client = client;
	pr_err("aw2013_i2c_probe ldo on\n");
	aw2013_ldo_gpio_output(1);
    msleep(20);

//    AW2013_i2c_write_reg(0x30, 0);              //led off   
//    AW2013_i2c_write_reg(0x01,0);
//    AW2013_i2c_write_reg(0x00, 0x55);                // Reset ==pdn
	aw2013_ready = 1;
	aw2013_ldo_gpio_output(0); //ISSI_led_function(0,0,50)    ;

#if 0
	led_flash_aw2013_test(0);///z
	led_flash_aw2013_power_low();
	led_flash_aw2013_charging_full();
	led_flash_aw2013_charging();
	led_flash_aw2013_unanswer_message_incall();
	//aw2013_create_sysfs(client);
#else
//      aw2103_breath_by_chip(0x2,2000,130,2000, 5000,5);
//		aw2103_breath_by_chip(R_LED_CHN, 0, 130, 0, 5000, 0);
//		aw2103_holdlight_by_chip(R_LED_CHN);
//		aw2103_breath_by_chip(R_LED_CHN | G_LED_CHN | B_LED_CHN, 0, 130, 0, 1040, 0);

#endif
	return 0;
}

static int aw2013_i2c_remove(struct i2c_client *client)
{
	aw2013_i2c_client = NULL;
	return 0;
}


static const struct i2c_device_id AW2013_i2c_id[] = {
	{ "aw2013_i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, AW2013_i2c_id);

//static struct i2c_board_info __initdata aw2013_i2c_hw={ I2C_BOARD_INFO("AW2013", (0x45))};


#ifdef CONFIG_OF
struct of_device_id aw2013_i2c_of_match[] = {
	{ .compatible = "mediatek,aw2013_i2c", },
	{},
};
#endif

static struct i2c_driver aw2013_i2c_driver = {
        .probe          = aw2013_i2c_probe,
        .remove         =  aw2013_i2c_remove,
        .id_table       = AW2013_i2c_id,
        .driver = {
                .owner  = THIS_MODULE,
                .name   = "aw2013_i2c",
#ifdef CONFIG_OF
              .of_match_table = aw2013_i2c_of_match,
#endif
        },
};
//end-Bee-20140417


//aw2013 device 
#ifdef CONFIG_OF
struct of_device_id aw2013_of_match[] = {
	{ .compatible = "mediatek,aw2013_dev", },
	{},
};
#endif

static int aw2013_probe(struct platform_device *pdev)
{
	int ret;
	ret = aw2013_gpio_init(pdev);
	pr_err("%s: aw2013_gpio_init=%d\n", __func__, ret);
	ret = i2c_add_driver(&aw2013_i2c_driver);
	pr_err("%s: i2c_add_driver=%d\n", __func__, ret);
	//aw2103_holdlight_by_chip(R_LED_CHN);
	return 0;
}

static int aw2013_remove(struct platform_device *pdev)
{
	i2c_del_driver(&aw2013_i2c_driver);
	return 0;
}

static struct platform_driver aw2013_driver = {
	.remove = aw2013_remove,
	.shutdown = NULL,
	.probe = aw2013_probe,
	.driver = {
			.name = "aw2013_dev",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = aw2013_of_match,
#endif
	},
};

static int __init hwctl_driver_init(void) 
{
printk("aw2013zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz\n");
	pr_err("%s: aw2013 driver init\n", __func__);
	if (platform_driver_register(&aw2013_driver) != 0) {
		pr_err("unable to register aw2013 driver.\n");
	}
	return 0;
}

/* should never be called */
static void __exit hwctl_driver_exit(void) 
{
	pr_err("MediaTek aw2013 driver exit\n");
	platform_driver_unregister(&aw2013_driver);
}

//hct-drv add for poweroff charger to show led.
static int last_onoff = -1;
static int last_mode = -1;

void aw2103_poweroff_indication(int onoff,int mode,int led_brightness)
{
    pr_err("%s: onoff=%d, mode=%d, last_onoff = %d. last_mode = %d.  \n", __func__, onoff, mode, last_onoff, last_mode);
    
    if((last_onoff != onoff) || (last_mode != mode)){
        ISSI_led_function(0, 0, led_brightness);
        ISSI_led_function(onoff, mode, led_brightness);
        last_onoff = onoff;
        last_mode = mode;
    }
}
//hct-drv add for poweroff charger to show led.



void ISSI_led_function(int onoff,int mode,int led_brightness)
{
    pr_err("%s: onoff=%d, mode=%d, led_brightness=%d\n", __func__, onoff, mode, led_brightness);
    if(!aw2013_ready){
        return;
    }

    
    if(onoff==0)
    {
        led_off_aw2013_mode(mode);
	//led_off_aw2013();
#if defined (__HCT_STANDBY_SHUTDOWN_AW2013_LDO_SUPPORT__)
#if __HCT_STANDBY_SHUTDOWN_AW2013_LDO_SUPPORT__
	aw2013_ldo_gpio_output(0);
#endif
#endif
    }
    else
    {
#if defined (__HCT_STANDBY_SHUTDOWN_AW2013_LDO_SUPPORT__)
#if __HCT_STANDBY_SHUTDOWN_AW2013_LDO_SUPPORT__
    led_off_aw2013_mode(mode);
	aw2013_ldo_gpio_output(0);
    mdelay(20);
	aw2013_ldo_gpio_output(1);
	mdelay(20);
#endif
#endif
       switch(mode)
       {
			//red 5s
            case 1:
			case 129:
				AW2013_LED_Blink(R_LED_CHN, 0, 130, 0, 4160, 0);
			break;
			//green 5s
			case 2:
			case 130:
				AW2013_LED_Blink(G_LED_CHN, 0, 130, 0, 4160, 0);
			break;
			//red 20s
            case 3:
			case 131:
				//aw2103_breath_by_chip(R_LED_CHN, 0, 130, 0, 8000, 0);
				AW2013_LED_Blink(B_LED_CHN, 0, 130, 0, 16640, 0);
			break;
			//cycle
            case 4:
			case 132:
//TODO				
				AW2013_LED_Blink(G_LED_CHN | B_LED_CHN | R_LED_CHN, 0, 130, 0, 4160, 0);
			break;
			//holdlight red
            case 5:
			case 133:
				aw2103_holdlight_by_chip(R_LED_CHN);   //5 4 blue
			break;
       	case 8:
			case 136:
				aw2103_holdlight_by_chip(G_LED_CHN);   //8 2 green.
			break;
            case 9:
            case 137:
            	aw2103_holdlight_by_chip(B_LED_CHN);   //9 1 red
			break;
			//holdlight red/green/bule
            case 6:
			case 134:
			case 127:
				aw2103_holdlight_by_chip(R_LED_CHN|G_LED_CHN|B_LED_CHN);
			break;
            case 7:
			case 135:
				aw2103_breath_by_chip(R_LED_CHN, 2080, 1040, 2080, 8320, 0); // custom for T325C-xfy-A5025
			break;
            case 10:
			case 138:
				aw2103_breath_by_chip(G_LED_CHN, 2080, 1040, 2080, 8320, 0); // custom for T325C-xfy-A5025
			break;
			case 11:
			case 139:
				aw2103_breath_by_chip(B_LED_CHN, 2080, 1040, 2080, 8320, 0); // custom for T325C-xfy-A5025
			break;
 			case 12:
			case 140:
				aw2103_breath_by_chip(R_LED_CHN|G_LED_CHN|B_LED_CHN, 2080, 1040, 2080, 8320, 0); // custom for T325C-xfy-A5025
			break;
            default:
                break;
       }
    }

}

module_init(hwctl_driver_init);
module_exit(hwctl_driver_exit);
MODULE_DESCRIPTION("Linux HW direct control driver");
MODULE_AUTHOR("David.wang(softnow@live.cn)");
MODULE_LICENSE("GPL");

#endif
   
