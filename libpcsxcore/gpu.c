/***************************************************************************
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 ***************************************************************************/

#include "gpu.h"
#include "psxdma.h"

void gpu_state_change(int what)
{
	enum psx_gpu_state state = what;
	switch (state)
	{
	case PGS_VRAM_TRANSFER_START:
		HW_GPU_STATUS &= ~SWAP32(PSXGPU_nBUSY);
		break;
	case PGS_VRAM_TRANSFER_END:
		HW_GPU_STATUS |= SWAP32(PSXGPU_nBUSY);
		break;
	case PGS_PRIMITIVE_START:
		HW_GPU_STATUS &= ~SWAP32(PSXGPU_nBUSY);
		GPUDMA_INT(200); // see gpuInterrupt
		break;
	}
}
