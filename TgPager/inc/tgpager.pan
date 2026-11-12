/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __TGPAGER_PAN__
#define __TGPAGER_PAN__

/** TgPager application panic codes */
enum TTgPagerPanics
    {
    ETgPagerUi = 1
    // add further panics here
    };

inline void Panic(TTgPagerPanics aReason)
    {
    _LIT(applicationName,"TgPager");
    User::Panic(applicationName, aReason);
    }

#endif // __TGPAGER_PAN__
