#include "shell_options.h"
#include "shell.h"
#include "csstrings.h"
#include "Logging.h"
#include "alephversion.h"
#include <SDL2/SDL_main.h>

int main(int argc, char **argv)
{
	// Print banner (don't bother if this doesn't appear when started from a GUI)
	char app_name_version[256];
	expand_app_variables(app_name_version, "Aleph One $appLongVersion$");
	printf("%s\n%s\n\n"
	  "オリジナルのコードは、Bungie Software <http://www.bungie.com/>によるものです。\n"
	  "この他にLoren Petrich, Chris Pruett, Rhys Hill氏らによって書かれています。\n"
	  "TCP/IP ネットワーク by Woody Zenfell\n"
	  "SDLポート by Christian Bauer <Christian.Bauer@uni-mainz.de>\n"
	  "日本語化 by saiten <http://www.isidesystem.net/>, ookawa_mi, Logue <http://logue.be/>, marathon.\n"
#if defined(__MACH__) && defined(__APPLE__)
	  "Mac OS X/SDLバージョンは、Chris Lovell, Alexander Strange, and Woody Zenfell氏らによって作られました。\n"
#endif
	  "\nこのプログラムは有用であることを願って頒布されますが、*全くの無保証 *です。\n"
	  "商業可能性の保証や特定目的への適合性は、言外に示されたものも 含め、全く存在しません。\n"
	  "詳しくはGNU 一般公衆利用許諾書をご覧ください。\n"
#if defined(__WIN32__)
	  // Windows are statically linked against SDL, so we have to include this:
	  "\nSimple DirectMedia Layer (SDL) ライブラリは、\n"
	  "GNU 一般公衆利用許諾書によってライセンスされています。\n"
	  "詳細については、COPYING.SDLを参考にしてください。\n"
#endif
#if !defined(DISABLE_NETWORKING)
	  "\nこのビルドは、ネットワークプレイが有効です。\n"
#endif
#ifdef HAVE_LUA
	  "\nこのビルドは、Luaスクリプトが有効です。\n"
#endif
	  , app_name_version, A1_HOMEPAGE_URL
    );

	shell_options.parse(argc, argv);

	auto code = 0;

	try {
		
		// Initialize everything
		initialize_application();

		for (std::vector<std::string>::iterator it = shell_options.files.begin(); it != shell_options.files.end(); ++it)
		{
			if (handle_open_document(*it))
			{
				break;
			}
		}

		// Run the main loop
		main_event_loop();

	} catch (std::exception &e) {
		try 
		{
			logFatal("Unhandled exception: %s", e.what());
		}
		catch (...) 
		{
		}
		code = 1;
	} catch (...) {
		try
		{
			logFatal("Unknown exception");
		}
		catch (...)
		{
		}
		code = 1;
	}

	try
	{
		shutdown_application();
	}
	catch (...)
	{

	}

	return code;
}