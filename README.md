wodin
=====

Wodin is a nearly-complete IMAP server.

It was created back in 2003 when I was hired by Simdesk specifically to write it.  I finished it 13 weeks later, and it
languished for lack of testing resources.  Eventually, I created a IMAP test fixture to test it and found errors in the
MIME parsing, but by February 2004 there were no known errors in it.  However, Simdesk had made a decision to not use a
home-grown IMAP server, so the code was set on the shelf and, eventually removed from the Simdesk source tree.

I kept a copy when the source was removed from the Simdesk source tree.  I thought it was pretty good code and that it
didn't deserve to be forgotten.  My intention was to eventually secure the rights to it, or at least to secure permission
to release it under an appropriate license so that people could see it and appreciate it.  I continued to work on it,
producing a thread pool template library and an Internet server library to replace the "Simdeskisms" that I was required
to use because it was originally a Simdesk product.  Those libraries were not derived from any Simdesk sources so I have
long since released them under the Apache license.

I had originally written this IMAP server to work with a newly-defined database-backed mailstore, but that seemed like
less and less of a good idea.  A coworker had expressed interest in it, but only if I implemented maildir as a mail store.
I had some interest in maildir, but I use mbox at home, so I implemented mbox but defined an interface for creating other
mail stores including maildir and whatever else caught my fancy.  I also intended to use the interface from the other
direction and create a Web service to access the same mail stores.  Someday.

Then, one day Simdesk went under and Mezeo acquired all of Simdesk's assets. At about that time, what I was then calling
wodin was getting to a point where it could be released and I asked about the status of the IMAP server.  I was told that
Mezeo had acquired all of Simdesk's valuable assets, and that did not include any IMAP server source.  The source I had
labored over for years was abandoned by them and I could do with it what I wished.  What I wished was to release it, and
so here it is.

It's not really ready for use.  I implemented the mboxrd format, while nearly everyone uses mboxo so it won't work with,
say, postfix in all the corner cases.  It shouldn't screw anything up, it just won't quite do the right thing sometimes.
A more serious issue is that mbox locking isn't yet implemented.  That WILL screw things up, so don't put this into a
live mail system.  I also haven't yet got around to implementing user-defined flags.  At first I didn't understand them
and then I got busy doing other things.  They're on the list, though.  Not having user-defined flags doesn't seem to hurt
much, but it's definitely a limitation.

Lastly, I just finished a major reorganization and I would be shocked if I hadn't introduced some significant errors as
part of that.  I'll be squashing those as time goes on.

I'm also intending to add unit tests all over.  That hasn't happened yet, either, but one reason for the reorganization 
I just finished is to make those tests easier.
