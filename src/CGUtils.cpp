// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "stdafx.h"

#include "CGUtils.h"

uint32_t CalcMipNumber( uint32_t resolution )
{
    return upper_power_of_two( resolution );
}
