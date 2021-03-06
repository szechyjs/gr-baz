/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * gr-baz by Balint Seeber (http://spench.net/contact)
 * Information, documentation & samples: http://wiki.spench.net/wiki/gr-baz
 * This file uses source from rtl-sdr: http://sdr.osmocom.org/trac/wiki/rtl-sdr
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rtl2832.h"
#include "memory.h"	// memset
#include "assert.h"	// assert
#include "math.h"	// pow
#include <stdarg.h>	// va_list
#include "string.h"	// strcasecmp

#include "rtl2832-tuner_e4000.h"
#include "rtl2832-tuner_fc0013.h"
#include "rtl2832-tuner_fc0012.h"
#include "rtl2832-tuner_fc2580.h"
#include "rtl2832-tuner_r820t.h"
#include "rtl2832-tuner_e4k.h"

///////////////////////////////////////////////////////////

#define CTRL_IN			(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
#define CTRL_OUT		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)

#define DEFAULT_CRYSTAL_FREQUENCY	28800000

#define DEFAULT_MIN_SAMPLE_RATE		900001
#define DEFAULT_MAX_SAMPLE_RATE		3200000

#define DEFAULT_LIBUSB_TIMEOUT		3000

///////////////////////////////////////////////////////////

int get_map_index(int value, const int* map, int pair_count)
{
	int i = 0;
	for (; i < pair_count; i++)
	{
		if (map[i*2 + 0] >= value)
		{
			if (map[i*2 + 0] > value)
				--i;

			break;
		}
	}

	if ((i == -1) || (i == pair_count))
		return i;

	return (i*2);
}

const char* libusb_result_to_string(int res)
{
	switch (res)
	{
		case LIBUSB_ERROR_TIMEOUT:
			return "the transfer timed out";
		case LIBUSB_ERROR_PIPE:
			return "the control request was not supported by the device";
		case LIBUSB_ERROR_NO_DEVICE:
			return "the device has been disconnected";
		case 0:
			return "no data was transferred";
		default:
			return "unknown return code";
	}
}

///////////////////////////////////////////////////////////

namespace RTL2832_NAMESPACE
{

///////////////////////////////////////////////////////////

/* ezcap USB 2.0 DVB-T/DAB/FM stick */
#define EZCAP_VID		0x0bda
#define EZCAP_PID		0x2838

#define HAMA_VID		0x0bda	// Same as ezcap
#define HAMA_PID		0x2832

/* Terratec NOXON DAB/DAB+ USB-Stick */
#define NOXON_VID		0x0ccd
#define NOXON_PID		0x00b3
#define NOXON_V2_PID	0x00e0
#define NOXON_V3_PID	0x00D7

/* Dexatek Technology Ltd. DK DVB-T Dongle */
#define DEXATEK_VID		0x1d19
#define DEXATEK_PID		0x1101	// Also Logilink, MSI
#define DEXATEK_V2_PID	0x1102
#define DEXATEK_V3_PID	0x1103

/* Peak */
#define PEAK_VID		0x1b80
#define PEAK_PID		0xd395

/* Ardata MyVision, Gigabyte GT-U7300 DVB-T */
#define ARDATA_VID		0x1b80	// Same as PEAK_VID
#define ARDATA_PID		0xd393

/* MyGica/G-Tek */
#define MYGICA_VID		0x1f4d
#define MYGICA_PID		0xb803

/* Lifeview */
#define LIFEVIEW_VID	0x1f4d	// Same as MYGICA_VID
#define LIVEVIEW_PID	0xc803

/* PROlectrix */
#define PROLECTRIX_VID	0x1f4d	// Same as Lifeview
#define PROLECTRIX_PID	0xd803

/* Terratec Cinergy T */
#define CINERGY_VID		0x0ccd	// Same as NOXON
#define CINERGY_PID		0x00a9
#define CINERGY_V3_PID	0x00d3

/* DIKOM HD */
#define DIKOM_VID		0x1b80	// Same as Peak
#define DIKOM_PID		0xd394

/* Twintech UT-40 */
#define TWINTECH_VID	0x1b80
#define TWINTECH_PID	0xd3a4

/* Genius TVGo DVB-T03 USB dongle (Ver. B) */
#define GENIUS_VID		0x0458
#define GENIUS_PID		0x707f

/* SVEON STV20 DVB-T USB & FM */
#define SVEON_VID		0x1b80
#define SVEON_PID		0xd39d

/* Compro Videomate */
#define COMPRO_VID		0x185b
#define COMPRO_V1_PID	0x0620
#define COMPRO_V2_PID	0x0650

#define GET_CREATOR_FN(c)	TUNERS_NAMESPACE::c::TUNER_FACTORY_FN_NAME
#define GET_PROBE_FN(c)		TUNERS_NAMESPACE::c::TUNER_PROBE_FN_NAME
#define ADD_TUNER(c)		{ #c, GET_CREATOR_FN(c), GET_PROBE_FN(c) }

static struct _rtl2832_tuner_info
{
	const char* name;
	tuner::CreateTunerFn factory;
	tuner::ProbeTunerFn probe;
} _rtl2832_tuners[] = {
	ADD_TUNER(e4k),	// Swapped this around with e4000
	ADD_TUNER(fc0013),
	ADD_TUNER(fc2580),
	ADD_TUNER(r820t),
	ADD_TUNER(fc0012),
	ADD_TUNER(e4000)
};

static DEVICE_INFO _rtl2832_devices[] = {	// Tuner does auto-detection (ignores creator hint) by default now!
	{ "ezcap EzTV",					EZCAP_VID,		EZCAP_PID, 		NULL/*GET_CREATOR_FN(e4000)*/	},
	// Use custom tuner name when creating device for ecap EZTV646 FC0013 but same PID: 2838
	{ "Terratec NOXON (rev 1)",		NOXON_VID,		NOXON_PID, 		GET_CREATOR_FN(fc0013)	},
	{ "Terratec NOXON (rev 2)", 	NOXON_VID,		NOXON_V2_PID, 	NULL/*GET_CREATOR_FN(e4000)*/	},
	{ "Terratec NOXON (rev 3)",		NOXON_VID,		NOXON_V3_PID, 	NULL/*GET_CREATOR_FN(e4000)*/	},
	{ "Hama nano",					HAMA_VID,		HAMA_PID, 		NULL/*GET_CREATOR_FN(e4000)*/	},
	{ "Dexatek Technology (rev 1)",	DEXATEK_VID,	DEXATEK_PID,	GET_CREATOR_FN(fc0013/*fc2580*/)	},	// Also Logilink
	{ "Dexatek Technology (rev 2)", DEXATEK_VID,	DEXATEK_V2_PID, GET_CREATOR_FN(fc0013)	},	// Also ZAAPA HD Tuner
	{ "Dexatek Technology (rev 3)", DEXATEK_VID,	DEXATEK_V3_PID,	GET_CREATOR_FN(fc0013/*fc2580*/)	},
	{ "Peak",						PEAK_VID,		PEAK_PID,		GET_CREATOR_FN(fc0012)	},
	{ "Ardata MyVision",			ARDATA_VID,		ARDATA_PID,		GET_CREATOR_FN(fc0012)	},
	{ "MyGica/G-Tek",				MYGICA_VID,		MYGICA_PID,		GET_CREATOR_FN(fc0012)	},
	{ "Lifeview",					LIFEVIEW_VID,	LIVEVIEW_PID,	GET_CREATOR_FN(fc0012)	},
	{ "Prolectrix",					PROLECTRIX_VID,	PROLECTRIX_PID,	GET_CREATOR_FN(fc0012)	},
	{ "Terratec Cinergy T (rev 1)", CINERGY_VID,	CINERGY_PID,	GET_CREATOR_FN(fc0012)	},
	{ "Rerratec Cinergy T (rev 3)", CINERGY_VID,	CINERGY_V3_PID },	// e4000
	{ "DIKOM HD",					DIKOM_VID,		DIKOM_PID,		GET_CREATOR_FN(fc0012)	},
	{ "Twintech",					TWINTECH_VID,	TWINTECH_PID },		// fc0013
	{ "Genius TVGo (rev 2)",		GENIUS_VID,		GENIUS_PID },
	{ "SVEON",						SVEON_VID,		SVEON_PID },		// fc0012
	{ "Compro Videomate U620F",		COMPRO_VID,		COMPRO_V1_PID },	// e4000
	{ "Compro Videomate U650F",		COMPRO_VID,		COMPRO_V2_PID },	// e4000
};

static struct _rtl2832_tuner_info* get_tuner_factory_by_name(const char* name)
{
	if (name == NULL)
		return NULL;

	for (int i = 0; i < (sizeof(_rtl2832_tuners)/sizeof(_rtl2832_tuners[0])); ++i)
	{
		struct _rtl2832_tuner_info* info = _rtl2832_tuners + i;

		if (
#ifdef _WIN32
			stricmp
#else
			strcasecmp
#endif // _WIN32
			(name, info->name) == 0)
			return info;
	}

	return NULL;
}

///////////////////////////////////////////////////////////

tuner::~tuner()
{
}

///////////////////////////////////////////////////////////

tuner_skeleton::tuner_skeleton(demod* p)
	: m_demod(p)
	, m_auto_gain_mode(false)
	, m_gain_mode(DEFAULT)
	, m_freq(0)
	, m_gain(0)
	, m_bandwidth(0)
{
	assert(p);
	
	memset(&m_params, 0x00, sizeof(m_params));
}

tuner_skeleton::~tuner_skeleton()
{
}

int tuner_skeleton::initialise(PPARAMS params /*= NULL*/)
{
	if (params)
		memcpy(&m_params, params, sizeof(m_params));
	
	return SUCCESS;
}

int tuner_skeleton::set_i2c_repeater(bool on /*= true*/, const char* function_name /*= NULL*/, int line_number /*= -1*/, const char* line /*= NULL*/)
{
	return m_demod->set_i2c_repeater(on, function_name, line_number, line);
}

int tuner_skeleton::i2c_read(uint8_t i2c_addr, uint8_t *buffer, int len)
{
	return m_demod->i2c_read(i2c_addr, buffer, len);
}

int tuner_skeleton::i2c_write(uint8_t i2c_addr, uint8_t *buffer, int len)
{
	return m_demod->i2c_write(i2c_addr, buffer, len);
}

int tuner_skeleton::i2c_write_reg(uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
	return m_demod->i2c_write_reg(i2c_addr, reg, val);
}

int tuner_skeleton::i2c_read_reg(uint8_t i2c_addr, uint8_t reg, uint8_t& data)
{
	return m_demod->i2c_read_reg(i2c_addr, reg, data);
}

///////////////////////////////////////////////////////////

demod::demod()
	: m_devh(NULL)
	, m_tuner(NULL)
	, m_libusb_init_done(false)
	, m_crystal_frequency(DEFAULT_CRYSTAL_FREQUENCY)
	, m_sample_rate(0)
	, m_current_info(NULL)
{
	memset(&m_params, 0x00, sizeof(m_params));
	
	m_dummy_tuner = new tuner_skeleton(this);
	m_tuner = m_dummy_tuner;

	m_sample_rate_range = std::make_pair(DEFAULT_MIN_SAMPLE_RATE, DEFAULT_MAX_SAMPLE_RATE);
}

demod::~demod()
{
	destroy();
	
	delete m_dummy_tuner;
}

int demod::check_libusb_result(int res, bool zero_okay, const char* function_name /*= NULL*/, int line_number /*= -1*/, const char* line /*= NULL*/)
{
	if ((res < 0) || ((zero_okay == false) && (res == 0)))
	{
		if (m_params.message_output)
		{
			m_params.message_output->on_log_message_ex(RTL2832_NAMESPACE::log_sink::LOG_LEVEL_ERROR,
				(m_params.verbose ? "libusb error: %s [%i] (%s:%i) \"%s\"\n" : "libusb: %s [%i]"),
				libusb_result_to_string(res),
				res,
				function_name,
				line_number,
				line);
		}
	}
	
	return res;
}

const char* demod::name() const
{
	if (m_current_info == NULL)
		return "(custom)";
	else if (m_current_info->name == NULL)
		return "(no name)";

	return m_current_info->name;
}

int demod::find_device()
{
	if (m_devh != NULL)
	{
		log("Releasing previous device before attempting to find a new one\n");
		
		destroy();
	}

	struct libusb_device_handle* devh = NULL;
	DEVICE_INFO* found = NULL;
	bool custom_id = true;
	for (int i = 0; i < (sizeof(_rtl2832_devices)/sizeof(DEVICE_INFO)); ++i)
	{
		DEVICE_INFO* info = _rtl2832_devices + i;
		
		if ((m_params.vid != 0) && (m_params.vid != info->vid))
			continue;
		if ((m_params.pid != 0) && (m_params.pid != info->pid))
			continue;

		if ((m_params.vid != 0) && (m_params.pid != 0) && (m_params.vid == info->vid) && (m_params.pid == info->pid))
			custom_id = false;

		devh = libusb_open_device_with_vid_pid(NULL, info->vid, info->pid);
		if (devh != NULL)
		{
			found = info;
			break;
		}
	}

	DEVICE_INFO custom;
	
	if ((devh == NULL) && (custom_id) && (m_params.vid != 0) && (m_params.pid != 0))
	{
		devh = libusb_open_device_with_vid_pid(NULL, m_params.vid, m_params.pid);
		if (devh)
		{
			memset(&custom, 0x00, sizeof(custom));

			custom.name	= "(custom)";
			custom.vid	= m_params.vid;
			custom.pid	= m_params.pid;

			found = &custom;
		}
		else
		{
			log("Could not find a device with custom ID: %04x:%04x\n", m_params.vid, m_params.pid);
			return FAILURE;
		}
	}

	if (devh == NULL)
	{
		log("Could not find a compatible device\n");
		return FAILURE;
	}

	assert(found);
	
	// FIXME: Fair to assume these will apply to custom devices?
	if (found->min_rate == 0)
		m_sample_rate_range.first = DEFAULT_MIN_SAMPLE_RATE;
	if (found->max_rate == 0)
		m_sample_rate_range.second = DEFAULT_MAX_SAMPLE_RATE;

	if (m_params.crystal_frequency)
		m_crystal_frequency = m_params.crystal_frequency;
	else if (found->crystal_frequency)
		m_crystal_frequency = found->crystal_frequency;
	else
		m_crystal_frequency = DEFAULT_CRYSTAL_FREQUENCY;

	int r;
	
	r = CHECK_LIBUSB_NEG_RESULT(libusb_claim_interface(devh, 0));
	if (r < 0)
	{
		destroy();
		return r;
	}

	m_devh = devh;

	RTL2832_NAMESPACE::tuner::CreateTunerFn factory = /*found->factory*/NULL;	// Auto-detect first

	struct _rtl2832_tuner_info* info = NULL;
	if (m_params.tuner_name[0])
	{
		info = get_tuner_factory_by_name(m_params.tuner_name);
		if (info == NULL)
			log("Failed to find custom tuner by name: \"%s\"\n", m_params.tuner_name);
		else
			factory = info->factory;
	}

	r = init_demod();
	if (r != SUCCESS)
	{
		log("\tCould not initialise device: \"%s\"\n", found->name);
		return r;
	}

	if (m_params.verbose)
		log("Successfully initialised demod: \"%s\"\n", found->name);

	if (factory == NULL)	// Probe
	{
		for (int i = 0; i < (sizeof(_rtl2832_tuners)/sizeof(_rtl2832_tuners[0])); ++i)
		{
			struct _rtl2832_tuner_info* info = _rtl2832_tuners + i;

			if (info->probe)
			{
				if (m_params.verbose)
					log("Probing \"%s\"...", info->name);

				int r = (info->probe)(this);
				if (r == SUCCESS)
				{
					if (m_params.verbose)
						log("found.\n", info->name);

					factory = info->factory;

					break;
				}
				else
				{
					if (m_params.verbose)
					{
						if (r == FAILURE)
							log("bad check value.\n");
						else
							log("not found.\n");
					}
				}
			}
		}

		if (factory == NULL)
		{
			if (found->factory)
			{
				log("Auto-probe failed, forcing hinted tuner\n");

				factory = found->factory;
			}
			else
				log("Could not find tuner automatically after probe\n");
		}
	}
	else
	{
		if (m_params.tuner_name[0])
		{
			if (m_params.verbose)
				log("Skipping auto-probe with custom tuner: \"%s\"\n", m_params.tuner_name);
		}
		else
		{
			if (m_params.verbose)
				log("Skipping auto-probe\n");
		}
	}

	tuner* t = NULL;
	if (factory)
	{
		t = (factory)(this);
		assert(t);
	}
	//else
	//	log("Device does not have tuner implemented interface\n");

	if (found != &custom)	// Don't store local variable
		m_current_info = found;

	log("Found RTL2832 device: %s (tuner: %s)\n", found->name, (t ? t->name() : "interface not implemented"));
	if (m_params.verbose)
	{
		log("\tSample rate range:\t%i - %i Hz\n"
			"\tCrystal frequency:\t%i Hz\n",
			(uint32_t)m_sample_rate_range.first, (uint32_t)m_sample_rate_range.second,
			m_crystal_frequency);
	}

	if (t)
		m_tuner = t;	// Store it for subsequent tuner initialisation

	return SUCCESS;
}

int demod::read_array(uint8_t block, uint16_t addr, uint8_t *array, uint8_t len)
{
	if (m_devh == NULL)
		return LIBUSB_ERROR_NO_DEVICE;
  
	uint16_t index = (block << 8);

	return /*CHECK_LIBUSB_RESULT*/(libusb_control_transfer(m_devh, CTRL_IN, 0, addr, index, array, len, 0));
}

int demod::write_array(uint8_t block, uint16_t addr, uint8_t *array, uint8_t len)
{
	if (m_devh == NULL)
		return LIBUSB_ERROR_NO_DEVICE;
  
	uint16_t index = (block << 8) | 0x10;

	return /*CHECK_LIBUSB_RESULT*/(libusb_control_transfer(m_devh, CTRL_OUT, 0, addr, index, array, len, 0));
}

int demod::i2c_write(uint8_t i2c_addr, uint8_t *buffer, int len)
{
	return write_array(IICB, i2c_addr, buffer, len);
}

int demod::i2c_read(uint8_t i2c_addr, uint8_t *buffer, int len)
{
	return read_array(IICB, i2c_addr, buffer, len);
}

int demod::i2c_write_reg(uint8_t i2c_addr, uint8_t reg, uint8_t val)
{
	uint16_t addr = i2c_addr;
	uint8_t data[2];

	data[0] = reg;
	data[1] = val;

	//CHECK_LIBUSB_RESULT_RETURN(write_array(IICB, addr, (uint8_t *)&data, 2));
	//return SUCCESS;

	return write_array(IICB, addr, (uint8_t *)&data, 2);
}

int demod::i2c_read_reg(uint8_t i2c_addr, uint8_t reg, uint8_t& data)
{
	uint16_t addr = i2c_addr;
	int r;

	//CHECK_LIBUSB_RESULT_RETURN(write_array(IICB, addr, &reg, 1));
	r = write_array(IICB, addr, &reg, 1);
	if (r <= 0)
		return r;

	//CHECK_LIBUSB_RESULT_RETURN(read_array(IICB, addr, &data, 1));
	//return SUCCESS;

	return read_array(IICB, addr, &data, 1);
}

int demod::read_reg(uint8_t block, uint16_t addr, uint8_t len, uint16_t& reg)
{
	if (m_devh == NULL)
		return LIBUSB_ERROR_NO_DEVICE;
  
	int r;
	unsigned char data[2];
	uint16_t index = (block << 8);

	r = /*CHECK_LIBUSB_RESULT*/(libusb_control_transfer(m_devh, CTRL_IN, 0, addr, index, data, len, 0));
	
	reg = (data[1] << 8) | data[0];

	return r;
}

int demod::write_reg(uint8_t block, uint16_t addr, uint16_t val, uint8_t len)
{
	if (m_devh == NULL)
		return LIBUSB_ERROR_NO_DEVICE;
  
	unsigned char data[2];

	uint16_t index = (block << 8) | 0x10;

	if (len == 1)
		data[0] = val & 0xff;
	else
		data[0] = val >> 8;

	data[1] = val & 0xff;

	return /*CHECK_LIBUSB_RESULT*/(libusb_control_transfer(m_devh, CTRL_OUT, 0, addr, index, data, len, 0));
}

int demod::demod_read_reg(uint8_t page, uint8_t addr, uint8_t len, uint16_t& reg)
{
	if (m_devh == NULL)
		return LIBUSB_ERROR_NO_DEVICE;
  
	int r;
	unsigned char data[2];

	uint16_t index = page;
	addr = (addr << 8) | 0x20;

	r = /*CHECK_LIBUSB_RESULT*/(libusb_control_transfer(m_devh, CTRL_IN, 0, addr, index, data, len, 0));

	reg = (data[1] << 8) | data[0];

	return r;
}

int demod::demod_write_reg(uint8_t page, uint16_t addr, uint16_t val, uint8_t len)
{
	if (m_devh == NULL)
		return LIBUSB_ERROR_NO_DEVICE;
  
	int r;
	unsigned char data[2];
	uint16_t index = 0x10 | page;
	addr = (addr << 8) | 0x20;

	if (len == 1)
		data[0] = val & 0xff;
	else
		data[0] = val >> 8;

	data[1] = val & 0xff;

	r = /*CHECK_LIBUSB_RESULT*/(libusb_control_transfer(m_devh, CTRL_OUT, 0, addr, index, data, len, 0));

	if (r >= 0)
	{
		uint16_t dummy;
		r = /*CHECK_LIBUSB_RESULT*/(demod_read_reg(0x0a, 0x01, 1, dummy));
	}
	
	return r;
}

int demod::set_sample_rate(uint32_t samp_rate, double* real_rate /*= NULL*/)
{
	uint16_t tmp;
	uint32_t rsamp_ratio;
	double _real_rate;

	//if (samp_rate > m_sample_rate_range.second)
	//	samp_rate = m_sample_rate_range.second;
	//else if (samp_rate < m_sample_rate_range.first)
	//	samp_rate = m_sample_rate_range.first;

	if (in_range(m_sample_rate_range, samp_rate) == false)
		return FAILURE;

	rsamp_ratio = ((uint64_t)m_crystal_frequency * (uint64_t)pow(2.0, 22.0)) / (uint64_t)samp_rate;
	rsamp_ratio &= ~3;

	if (rsamp_ratio == 0)
	{
		if (real_rate)
			(*real_rate) = 0.0;
		return FAILURE;
	}

	_real_rate = (m_crystal_frequency * pow(2.0, 22.0)) / (double)rsamp_ratio;

	tmp = (rsamp_ratio >> 16);
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x9f, tmp, 2));

	tmp = rsamp_ratio & 0xffff;
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0xa1, tmp, 2));
	
	m_sample_rate = _real_rate;

	if (real_rate)
		(*real_rate) = _real_rate;

	return SUCCESS;
}

int demod::set_i2c_repeater(bool on, const char* function_name /*= NULL*/, int line_number /*= -1*/, const char* line /*= NULL*/)
{
	return /*CHECK_LIBUSB_RESULT*/CHECK_LIBUSB_RESULT_EX(demod_write_reg(1, 0x01, (on ? 0x18 : 0x10), 1), function_name, line_number, line);
}

int demod::set_gpio_output(uint8_t gpio)
{
	uint16_t reg;
	gpio = 1 << gpio;

	CHECK_LIBUSB_RESULT_RETURN(read_reg(SYSB, GPD, 1, reg));
	CHECK_LIBUSB_RESULT_RETURN(write_reg(SYSB, GPO, reg & ~gpio, 1));
	CHECK_LIBUSB_RESULT_RETURN(read_reg(SYSB, GPOE, 1, reg));
	CHECK_LIBUSB_RESULT_RETURN(write_reg(SYSB, GPOE, reg | gpio, 1));

	return SUCCESS;
}

int demod::set_gpio_bit(uint8_t gpio, int val)
{
	uint16_t reg;

	gpio = 1 << gpio;
	CHECK_LIBUSB_RESULT_RETURN(read_reg(SYSB, GPO, 1, reg));
	reg = val ? (reg | gpio) : (reg & ~gpio);
	CHECK_LIBUSB_RESULT(write_reg(SYSB, GPO, reg, 1));

	return SUCCESS;
}

void demod::log(const char* message, ...)
{
	if (m_params.message_output == NULL)
		return;

	va_list args;
	va_start(args, message);
	
	m_params.message_output->on_log_message_va(RTL2832_NAMESPACE::log_sink::LOG_LEVEL_DEFAULT, message, args);
}

int demod::initialise(PPARAMS params /*= NULL*/)
{
	if (params)
		memcpy(&m_params, params, sizeof(m_params));

	//////////////////////////////////////
	
	if (m_params.default_timeout == 0)
		m_params.default_timeout = DEFAULT_LIBUSB_TIMEOUT;
	else if (m_params.default_timeout < 0)
	{
		if (m_params.verbose)
			log("USB transfer wait disabled (poll mode)\n");
		m_params.default_timeout = 0;
	}
	else
	{
		if (m_params.verbose)
			log("Custom USB transfer timeout: %i ms\n", m_params.default_timeout);
	}

	//////////////////////////////////////
	
	int r;

	if (m_libusb_init_done == false)
	{
		r = CHECK_LIBUSB_NEG_RESULT(libusb_init(NULL));
		if (r < 0)
		{
			log("\tFailed to initialise libusb\n");
			return r;
		}
		
		m_libusb_init_done = true;
	}

	r = find_device();
	if (r != SUCCESS)
	{
		destroy();
		return r;
	}

/*	if (m_params.use_tuner_params == false)
	{
		m_params.tuner_params.verbose = m_params.verbose;
		m_params.tuner_params.message_output = m_params.message_output;
	}
*/
	tuner::PARAMS tuner_params;
	if (m_params.tuner_params == NULL)
	{
		memset(&tuner_params, 0x00, sizeof(tuner_params));
		tuner_params.message_output = m_params.message_output;
		tuner_params.verbose = m_params.verbose;
	}

/*	if (m_tuner->initialise(&m_params.tuner_params) != SUCCESS)
	{
		log("\tCould not initialise tuner\n");
		destroy();
		return FAILURE;
	}
*/
	if (m_tuner->initialise((m_params.tuner_params ? m_params.tuner_params : &tuner_params)) != SUCCESS)
	{
		log("\tFailed to initialise tuner\n");
		destroy();
		return FAILURE;
	}

	return SUCCESS;
}

int demod::reset()
{
	/* reset endpoint before we start reading */
	CHECK_LIBUSB_RESULT_RETURN(write_reg(USBB, USB_EPA_CTL, 0x1002, 2));
	CHECK_LIBUSB_RESULT_RETURN(write_reg(USBB, USB_EPA_CTL, 0x0000, 2));

	return SUCCESS;
}

void demod::destroy()
{
	write_reg(SYSB, DEMOD_CTL, 0x20, 1);	// Poweroff demodulator and ADCs

	if ((m_tuner) && (m_tuner != m_dummy_tuner))
	{
		delete m_tuner;
		m_tuner = m_dummy_tuner;
	}
	
	if (m_devh != NULL)
	{
		libusb_release_interface(m_devh, 0);
		libusb_close(m_devh);
		m_devh = NULL;
	}

	if (m_libusb_init_done)
	{
		libusb_exit(NULL);
		m_libusb_init_done = false;
	}
}

int demod::init_demod()
{
	unsigned int i;

	/* default FIR coefficients used for DAB/FM by the Windows driver,
	 * the DVB driver uses different ones */
	static uint8_t default_fir_coeff[RTL2832_FIR_COEFF_COUNT] = {
		0xca, 0xdc, 0xd7, 0xd8, 0xe0, 0xf2, 0x0e, 0x35, 0x06, 0x50,
		0x9c, 0x0d, 0x71, 0x11, 0x14, 0x71, 0x74, 0x19, 0x41, 0x00,
	};

	/* initialize USB */
	CHECK_LIBUSB_RESULT_RETURN(write_reg(USBB, USB_SYSCTL, 0x09, 1));
	CHECK_LIBUSB_RESULT_RETURN(write_reg(USBB, USB_EPA_MAXPKT, 0x0002, 2));
	CHECK_LIBUSB_RESULT_RETURN(write_reg(USBB, USB_EPA_CTL, 0x1002, 2));

	/* poweron demod */
	CHECK_LIBUSB_RESULT_RETURN(write_reg(SYSB, DEMOD_CTL_1, 0x22, 1));
	CHECK_LIBUSB_RESULT_RETURN(write_reg(SYSB, DEMOD_CTL, 0xe8, 1));

	/* reset demod (bit 3, soft_rst) */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x01, 0x14, 1));
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x01, 0x10, 1));

	/* disable spectrum inversion and adjacent channel rejection */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x15, 0x00, 1));
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x16, 0x0000, 2));

	/* set IF-frequency to 0 Hz */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x19, 0x0000, 2));
	
	uint8_t* fir_coeff = (m_params.use_custom_fir_coefficients ? m_params.fir_coeff : default_fir_coeff);

	if ((m_params.use_custom_fir_coefficients) && (m_params.verbose))
		log("Using custom FIR coefficients\n");

	/* set FIR coefficients */
	for (i = 0; i < RTL2832_FIR_COEFF_COUNT; i++)
		CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x1c + i, fir_coeff[i], 1));

	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(0, 0x19, 0x25, 1));

	/* init FSM state-holding register */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x93, 0xf0, 1));

	/* disable AGC (en_dagc, bit 0) */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0x11, 0x00, 1));

	/* disable PID filter (enable_PID = 0) */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(0, 0x61, 0x60, 1));

	/* opt_adc_iq = 0, default ADC_I/ADC_Q datapath */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(0, 0x06, 0x80, 1));

	/* Enable Zero-IF mode (en_bbin bit), DC cancellation (en_dc_est),
	 * IQ estimation/compensation (en_iq_comp, en_iq_est) */
	CHECK_LIBUSB_RESULT_RETURN(demod_write_reg(1, 0xb1, 0x1b, 1));
	
	return SUCCESS;
}

int demod::read_samples(unsigned char* buffer, uint32_t buffer_size, int* bytes_read, int timeout /*= -1*/)
{
	assert(buffer);
	assert(buffer_size > 0);
	assert(bytes_read);
	
	return libusb_bulk_transfer(m_devh, 0x81, buffer, buffer_size, bytes_read, ((timeout < 0) ? m_params.default_timeout : timeout));
}

}	// namespace rtl2832
