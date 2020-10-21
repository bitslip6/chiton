#include <stdint.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <algorithm>
#include <map>
#include <utility>

enum class adi9036_item_bits_access {
   read,
   write,
   readwrite
};
class adi9036_item_bits {
public:  
  uint8_t start_bit;
  uint8_t end_bit;
  std::string description;
  uint64_t init_value;
  adi9036_item_bits_access access;
};

class adi9036_item {
public:
    uint16_t address;
    std::map<std::string,adi9036_item_bits> bits; 
};

std::map<std::string,adi9036_item_bits> adi9036_generic;

std::map<std::string,adi9036_item_bits> adi9036_afemisc {
   { "DC_BYPASS", { 2,2,"Disable dc restire during PBLK", 0x0, adi9036_item_bits_access::read } },
   { "PBLK_LVL",  { 1,1,"PBLK level", 0x0, adi9036_item_bits_access::readwrite }},
   { "CLPENABLE", { 0,0,"Black clamp enable", 0x1, adi9036_item_bits_access::readwrite }},
};


std::map<std::string, adi9036_item> adi9036_map  {
  { "AFEMISC", { 0xc000 ,  adi9036_afemisc }  },
  {  "CDSGAIN", { 0xc001 ,  adi9036_generic }  },

  {  "VGAGAIN", { 0xc002 ,  adi9036_generic  } },
{ "CLAMPLEVEL", { 0xc003 ,  adi9036_generic  } },
{ "CLSPEED", { 0xc004 ,  adi9036_generic  } },
{ "INPUT_EN1", {0xc015 ,  adi9036_generic  } },
// page 63
{ "XVSEL", {0xc01b ,  adi9036_generic  } },
{ "STANDBY", {0xc020 ,  adi9036_generic  } },
// page 64
{ "CLIDIVIDE", {0xc021 ,  adi9036_generic  } },
{ "SW_RST", {0xc022 ,  adi9036_generic  } },
{ "INPUT_EN2", {0xc023 ,  adi9036_generic  } },
{ "OUTPUT_EN", {0xc024 ,  adi9036_generic  } },
{ "RSTB_DISABLE", {0xc025 ,  adi9036_generic  } },
{ "STARTUP", {0xc026 ,  adi9036_generic  } },
{ "POLARITY", {0xc027 ,  adi9036_generic  } },
// page 65
{ "VDRIVER", {0xc028 ,  adi9036_generic  } },
{ "STARTUP_2", {0xc02c ,  adi9036_generic  } },
{ "GPOCFG", {0xc02f ,  adi9036_generic  } },
{ "CLI_SEL", {0xc030 ,  adi9036_generic  } },
{ "SCK_SEL", {0xc031 ,  adi9036_generic  } },
{ "MANUAL_UPDT", { 0xc032 ,  adi9036_generic  } },
{ "UPDT_SEL", { 0xc033 ,  adi9036_generic  } },
{ "I2C_KEY", { 0xc034 ,  adi9036_generic  } },
{ "I2C_ADDR", { 0xc035 ,  adi9036_generic  } },
// page 66
{ "H1_LOC_A", {0xc038 ,  adi9036_generic  } },
{ "H1_LOC_B", {0xc039 ,  adi9036_generic  } },
{ "H2_LOC_A", {0xc03a ,  adi9036_generic  } },
{ "H2_LOC_B", {0xc03b ,  adi9036_generic  } },
{ "H3_LOC_A", {0xc03c ,  adi9036_generic  } },
{ "H3_LOC_B", {0xc03d ,  adi9036_generic  } },
{ "H4_LOC", {0xc03e ,  adi9036_generic  } },
{ "H5_LOC", {0xc03f ,  adi9036_generic  } },
{ "HL_LOC_A", {0xc040 ,  adi9036_generic  } },
{ "HL_LOC_B", {0xc041 ,  adi9036_generic  } },
{ "RG_LOC_A", {0xc042 ,  adi9036_generic  } },
//page 67
{ "RG_LOC_B", {0xc043 ,  adi9036_generic  } },
{ "SHP_LOC", { 0xc044 ,  adi9036_generic  } },
{ "SHD_LOC", { 0xc046 ,  adi9036_generic  } },
{ "HBLK_RETIME", {0xc04f ,  adi9036_generic  } },
// page68
{ "HBLK_POL", {0xc050 ,  adi9036_generic  } },
{ "H_FINE_RETIME", {0xc051 ,  adi9036_generic  } },
{ "HL_FINE_RETIME", {0xc052 ,  adi9036_generic  } },
{ "TC_PDN_CTRL1", {0xc053 ,  adi9036_generic  } },
{ "TC_PDN_CTRL2", {0xc054 ,  adi9036_generic  } },
{ "H1DRV_A", {0xc055 ,  adi9036_generic  } },
{ "H1DRV_B", {0xc056 ,  adi9036_generic  } },
//page70
{ "H2DRV_A", {0xc057 ,  adi9036_generic  } },
{ "H2DRV_B", {0xc058 ,  adi9036_generic  } },
{ "H3DRV_A", {0xc059 ,  adi9036_generic  } },
{ "H3DRV_B", {0xc05a ,  adi9036_generic  } },
{ "H4DRV", {0xc05b ,  adi9036_generic  } },
{ "H5DRV", {0xc05c ,  adi9036_generic  } },
{ "HLDRV", {0xc05d ,  adi9036_generic  } },
//page71
{ "RGDRV", {0xc05e ,  adi9036_generic  } },
{ "UPDATE_ADDR_0X", {0xc060 ,  adi9036_generic  } },
{ "UPDATE_ADDR_1X", {0xc061 ,  adi9036_generic  } },
{ "UPDATE_ADDR_2X", {0xc062 ,  adi9036_generic  } },
{ "UPDATE_ADDR_3X", {0xc063 ,  adi9036_generic  } },
{ "UPDATE_ADDR_4X", {0xc064 ,  adi9036_generic  } },
{ "UPDATE_ADDR_5X", {0xc065 ,  adi9036_generic  } },

// page74
{ "XV_MUX_SELECT01", {0xc070 ,  adi9036_generic  } },
{ "XV_MUX_SELECT23", {0xc071 ,  adi9036_generic  } },
{ "XV_MUX_SELECT45", {0xc072 ,  adi9036_generic  } },
{ "XV_MUX_SELECT67", {0xc073 ,  adi9036_generic  } },
{ "XV_MUX_SELECT89", {0xc074 ,  adi9036_generic  } },
{ "XV_MUX_SELECT1011", {0xc075 ,  adi9036_generic  } },
{ "XV_MUX_SELECT1213", {0xc076 ,  adi9036_generic  } },
// page75
{ "XV_MUX_SELECT1415", {0xc077 ,  adi9036_generic  } },
{ "OUTEN_UPDATE_SEL", {0xc07d ,  adi9036_generic  } },
{ "TG_OUTEN", {0xc07f ,  adi9036_generic  } },
{ "XVOUTEN0", {0xc080 ,  adi9036_generic  } },
{ "XVOUTEN1", {0xc081 ,  adi9036_generic  } },
{ "XSGOUTEN", {0xc082 ,  adi9036_generic  } },
{ "HBLKOUTEN", {0xc083 ,  adi9036_generic  } },
{ "LDPOSBLKOUTEN", {0xc084 ,  adi9036_generic  } },
{ "LDNEGBLKOUTEN", {0xc085 ,  adi9036_generic  } },
{ "GPOOUTEN", {0xc086 ,  adi9036_generic  } },
{ "MISCOUTEN", {0xc087 ,  adi9036_generic  } },
{ "XVOUTSTBY0", {0xc088 ,  adi9036_generic  } },
{ "XVOUTSTBY1", {0xc089 ,  adi9036_generic  } },
{ "XSGOUTSTBY", {0xc08a ,  adi9036_generic  } },

#if 0
// page76
{ "", {0xc0 ,  adi9036_generic  } },

// page77
{ "", {0xc0 ,  adi9036_generic  } },

// page78
{ "", {0xc0 ,  adi9036_generic  } },

// page79
{ "", {0xc0 ,  adi9036_generic  } },

// page80
{ "", {0xc0 ,  adi9036_generic  } },

// page81
{ "", {0xc0 ,  adi9036_generic  } },

// page82
{ "", {0xc0 ,  adi9036_generic  } },

// page83
{ "", {0xc0 ,  adi9036_generic  } },

// page84
{ "", {0xc0 ,  adi9036_generic  } },

// page85
{ "", {0xc0 ,  adi9036_generic  } },

// page86
{ "", {0xc0 ,  adi9036_generic  } },

// page87
{ "", {0xc0 ,  adi9036_generic  } },

// page88
{ "", {0xc0 ,  adi9036_generic  } },
#endif


{ "TEST_DATA", { 0xc0fc ,  adi9036_generic  } },

};







void adi9036_i2c_read(std::pair<std::string,adi9036_item> item){

  // open the bus
  int fd = open("/dev/i2c-3", O_RDWR);
  if(fd < 0){
    fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
    return;
  }
  // select slave
  
  if (ioctl(fd, I2C_SLAVE_FORCE, 0x64) < 0) {
    fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
    return;
  }

	unsigned char reg_buf[2];
	int ret;
	reg_buf[0] = ( item.second.address >> 0) & 0xFF;
	reg_buf[1] = ( item.second.address >> 8) & 0xFF;
	ret = write(fd, reg_buf, 2);
	if (ret < 0) {
        fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
		return;
	}
	unsigned char val_buf[2];
	ret = read(fd, val_buf, 2);
	if (ret < 0) {
        fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
		return;
	}

  printf("name: %-20s reg:%04x val:%02x%02x\n",item.first.c_str(),item.second.address,val_buf[0],val_buf[1]);
  

  close(fd);

}


int main(int argc,char** argv){

    std::for_each( std::begin(adi9036_map), std::end(adi9036_map), adi9036_i2c_read);

  return 0;
}






