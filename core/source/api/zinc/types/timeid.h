/***************************************************************************//**
 * FILE : timeid.h
 *
 */
/* OpenCMISS-Zinc Library
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CMZN_TIMEID_H__
#define CMZN_TIMEID_H__

/***************************************************************************//**
 * A handle to zinc time notifier. This notifier provides a concept of time to
 * Cmgui, it will notify its client when time has changed if a callback is setup
 * for this notifier. time notifier normally receives its callback from a
 * time keeper. See cmzn_time_keeper_add_time_notifier function.
 */
	struct cmzn_time_notifier;
	typedef struct cmzn_time_notifier *cmzn_time_notifier_id;

#endif /* CMZN_TIME_ID_H */
