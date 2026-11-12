/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __HELLON70_PAN__
#define __HELLON70_PAN__

/** HelloN70 application panic codes */
enum THelloN70Panics
    {
    EHelloN70Ui = 1
    // add further panics here
    };

inline void Panic(THelloN70Panics aReason)
    {
    _LIT(applicationName,"HelloN70");
    User::Panic(applicationName, aReason);
    }

#endif // __HELLON70_PAN__
