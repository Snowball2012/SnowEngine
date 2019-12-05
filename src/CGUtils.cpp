#include "stdafx.h"

#include "CGUtils.h"

uint32_t CalcMipNumber( uint32_t resolution )
{
    return upper_power_of_two( resolution );
}
