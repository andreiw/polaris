/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)sfx4500_props.c	1.2	06/05/31 SMI"

#include <stdlib.h>
#include "sata.h"
#include "sfx4500_props.h"

struct sata_machine_specific_properties SFX4500_machprops =
{
"Sun Fire X4500",
SUN_FIRE_X4500_PROPERTIES,
{
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:0",
	"HD_ID_0",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=0" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=90 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=90 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=90 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=90 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=90 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=90 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:1",
	"HD_ID_12",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=12" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=102 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=102 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=102 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=102 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=102 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=102 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:2",
	"HD_ID_24",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=24" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=114 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=114 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=114 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=114 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=114 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=114 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:3",
	"HD_ID_36",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=36" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=126 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=126 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=126 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=126 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=126 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=126 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:4",
	"HD_ID_1",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=1" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=91 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=91 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=91 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=91 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=91 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=91 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:5",
	"HD_ID_13",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=13" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=103 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=103 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=103 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=103 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=103 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=103 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:6",
	"HD_ID_25",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=25" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=115 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=115 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=115 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=115 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=115 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=115 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@4/pci11ab,11ab@1:7",
	"HD_ID_37",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=37" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=127 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=127 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=127 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=127 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=127 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=127 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:0",
	"HD_ID_2",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=2" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=92 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=92 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=92 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=92 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=92 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=92 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:1",
	"HD_ID_14",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=14" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=104 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=104 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=104 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=104 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=104 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=104 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:2",
	"HD_ID_26",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=26" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=116 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=116 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=116 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=116 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=116 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=116 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:3",
	"HD_ID_38",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=38" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=128 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=128 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=128 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=128 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=128 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=128 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:4",
	"HD_ID_3",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=3" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=93 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=93 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=93 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=93 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=93 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=93 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:5",
	"HD_ID_15",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=15" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=105 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=105 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=105 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=105 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=105 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=105 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:6",
	"HD_ID_27",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=27" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=117 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=117 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=117 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=117 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=117 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=117 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@1,0/pci1022,7458@3/pci11ab,11ab@1:7",
	"HD_ID_39",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=39" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=129 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=129 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=129 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=129 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=129 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=129 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:0",
	"HD_ID_4",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=4" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=94 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=94 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=94 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=94 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=94 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=94 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:1",
	"HD_ID_16",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=16" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=106 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=106 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=106 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=106 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=106 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=106 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:2",
	"HD_ID_28",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=28" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=118 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=118 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=118 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=118 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=118 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=118 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:3",
	"HD_ID_40",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=40" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=130 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=130 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=130 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=130 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=130 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=130 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:4",
	"HD_ID_5",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=5" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=95 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=95 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=95 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=95 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=95 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=95 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:5",
	"HD_ID_17",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=17" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=107 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=107 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=107 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=107 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=107 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=107 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:6",
	"HD_ID_29",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=29" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=119 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=119 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=119 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=119 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=119 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=119 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@8/pci11ab,11ab@1:7",
	"HD_ID_41",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=41" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=131 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=131 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=131 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=131 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=131 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=131 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:0",
	"HD_ID_6",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=6" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=96 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=96 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=96 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=96 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=96 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=96 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:1",
	"HD_ID_18",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=18" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=108 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=108 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=108 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=108 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=108 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=108 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:2",
	"HD_ID_30",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=30" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=120 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=120 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=120 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=120 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=120 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=120 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:3",
	"HD_ID_42",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=42" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=132 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=132 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=132 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=132 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=132 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=132 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:4",
	"HD_ID_7",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=7" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=97 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=97 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=97 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=97 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=97 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=97 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:5",
	"HD_ID_19",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=19" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=109 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=109 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=109 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=109 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=109 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=109 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:6",
	"HD_ID_31",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=31" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=121 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=121 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=121 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=121 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=121 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=121 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@2,0/pci1022,7458@7/pci11ab,11ab@1:7",
	"HD_ID_43",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=43" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=133 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=133 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=133 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=133 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=133 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=133 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:0",
	"HD_ID_8",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=8" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=98 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=98 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=98 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=98 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=98 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=98 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:1",
	"HD_ID_20",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=20" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=110 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=110 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=110 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=110 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=110 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=110 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:2",
	"HD_ID_32",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=32" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=122 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=122 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=122 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=122 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=122 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=122 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:3",
	"HD_ID_44",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=44" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=134 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=134 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=134 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=134 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=134 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=134 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:4",
	"HD_ID_9",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=9" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=99 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=99 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=99 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=99 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=99 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=99 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:5",
	"HD_ID_21",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=21" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=111 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=111 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=111 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=111 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=111 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=111 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:6",
	"HD_ID_33",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=33" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=123 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=123 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=123 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=123 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=123 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=123 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@2/pci11ab,11ab@1:7",
	"HD_ID_45",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=45" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=135 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=135 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=135 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=135 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=135 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=135 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:0",
	"HD_ID_10",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=10" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=100 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=100 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=100 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=100 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=100 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=100 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:1",
	"HD_ID_22",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=22" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=112 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=112 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=112 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=112 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=112 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=112 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:2",
	"HD_ID_34",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=34" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=124 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=124 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=124 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=124 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=124 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=124 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:3",
	"HD_ID_46",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=46" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=136 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=136 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=136 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=136 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=136 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=136 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:4",
	"HD_ID_11",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=11" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=101 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=101 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=101 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=101 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=101 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=101 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:5",
	"HD_ID_23",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=23" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=113 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=113 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=113 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=113 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=113 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=113 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:6",
	"HD_ID_35",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=35" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=125 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=125 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=125 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=125 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=125 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=125 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	"/devices/pci@0,0/pci1022,7458@1/pci11ab,11ab@1:7",
	"HD_ID_47",
	{
		{ SFX4500_FRU_UPDATE_ACTION,	"ipmi:fru gid=3 hdd=47" },
		{ NULL,				NULL }
	},
	{
		{ "+PRSNT", "ipmi:state sid=137 amask=0x0001" },
		{ "-PRSNT", "ipmi:state sid=137 dmask=0x0001" },
		{ "+OK2RM", "ipmi:state sid=137 amask=0x0008" },
		{ "-OK2RM", "ipmi:state sid=137 dmask=0x0008" },
		{ "+FAULT", "ipmi:state sid=137 amask=0x0002" },
		{ "-FAULT", "ipmi:state sid=137 dmask=0x0002" },
		{ NULL,		NULL }
	}
},
{
	/* The last entry *MUST* have NULL in the .ap_path member */
	NULL
}
} /* end of sata_device struct array */,
/* These rules are common to all disks */
{
{ "absent>present",		"+PRSNT&+OK2RM" },
{ "present>configured",		"+PRSNT&-OK2RM" },
{ "configured>unconfigured",	"+OK2RM" },
{ "unconfigured>configured",	"-OK2RM" },
{ "unconfigured>absent",	"-OK2RM&-PRSNT" },
{ "configured>absent",		"-OK2RM&-PRSNT" },
{ "present>absent",		"-OK2RM&-PRSNT" },
{ NULL,				NULL }
} /* end of action_rules struct array */
}; /* End of SFX4500_machprops */
