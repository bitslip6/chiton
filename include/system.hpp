/******************************************************************************
 *
 * @file            system.hpp
 *
 * @brief           System-level functions.
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
 *
 * @created         7/9/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

#ifndef SYSTEM_HPP__
#define SYSTEM_HPP__

//---------------------------------------------------------------------
// Types
//---------------------------------------------------------------------

extern "C" {
	typedef void (*shutdown_fn_t)(void);
}

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace sys {
	/* Initialize signal handlers. */
	void init_signal_handlers();

	extern "C" {

		/* Exit the program with a specific code and message. */
		void panic(int code, const char * format, ...);

		/* Handler for CTRL-C (SIGINT). */
		void ctrl_c_handler(int signum);

		/* Register a function to be called during shutdown, such as from an
		 * interrupt handler.
		 */
		void register_shutdown_fn(shutdown_fn_t func);

		/* Shutdown with a specific exit code, executing all registered shutdown
		 * functions beforehand.
		 */
		void shutdown_now(int exit_code = 0);

		DisplayBuffers* SystemInit(u16 width, u16 height);

	}    // extern "C"
}    // namespace sys

#endif /* SYSTEM_HPP__ */
