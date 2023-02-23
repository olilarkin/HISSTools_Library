
#pragma once

enum ConvolveError
{
	CONVOLVE_ERR_NONE = 0,
	CONVOLVE_ERR_IN_CHAN_OUT_OF_RANGE,
	CONVOLVE_ERR_OUT_CHAN_OUT_OF_RANGE,
	CONVOLVE_ERR_MEM_UNAVAILABLE,
	CONVOLVE_ERR_MEM_ALLOC_TOO_SMALL,
	CONVOLVE_ERR_TIME_IMPULSE_TOO_LONG,
	CONVOLVE_ERR_TIME_LENGTH_OUT_OF_RANGE,
	CONVOLVE_ERR_PARTITION_LENGTH_TOO_LARGE,
	CONVOLVE_ERR_FFT_SIZE_MAX_TOO_SMALL,
	CONVOLVE_ERR_FFT_SIZE_MAX_TOO_LARGE,
	CONVOLVE_ERR_FFT_SIZE_MAX_NON_POWER_OF_TWO,
	CONVOLVE_ERR_FFT_SIZE_OUT_OF_RANGE,
	CONVOLVE_ERR_FFT_SIZE_NON_POWER_OF_TWO,
};
