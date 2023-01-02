#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>
#include <iostream>


using namespace sword;
using namespace std;


int main(int argc, char **argv)
{

	SWMgr library;

	SWModule *book = library.getModule("KJV");

	VerseKey *vk = (VerseKey *) book->getKey();
	for (;!vk->popError();vk->setChapter(vk->getChapter()+1)) {
		vk->setVerse(vk->getVerseMax());
		SWBuf text = book->stripText();
		text = text.trim();
		if (text.endsWith(",")) {
			cout << vk->getText() << ":\n\n";
			cout << text << endl;
		}
	}

	return 0;
}
