Hey so here is my miniFAT file system. 

To use it please download a hex editor. This way you can view what is happening to the .img file.

I suggest you make a back up copy of the image file so that if you mess it up you can quickly
grab a new one.

*Known bug, 
    so when writing consecutivly when it gets to block 23 it begins to skip blocks from the queue
    I have noidea why this is and leave it as an excersise to someone else who wishes to view this 
    code. I still think it is a good way of visualising what is going on inside your disk when you
    wrtie to it.
    
Hope you all enjoy it
