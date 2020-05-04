#!/usr/bin/env python3

import concurrent.futures
import os
import requests
import sys
import time

# Functions


def usage(status=0):
    progname = os.path.basename(sys.argv[0])
    print(f'''Usage: {progname} [-h HAMMERS -t THROWS] URL
    -h  HAMMERS     Number of hammers to utilize (1)
    -t  THROWS      Number of throws per hammer  (1)
    -v              Display verbose output
    ''')
    sys.exit(status)


def hammer(url, throws, verbose, hid):
    ''' Hammer specified url by making multiple throws (ie. HTTP requests).

    - url:      URL to request
    - throws:   How many times to make the request
    - verbose:  Whether or not to display the text of the response
    - hid:      Unique hammer identifier

    Return the average elapsed time of all the throws.
    '''
    # Total time for all requests
    totalTime = 0.0

    for i in range(throws):
        # Time per request
        startTime = time.time()
        response = requests.get(url)
        endTime = time.time() - startTime

        totalTime += endTime
        if(verbose):
            print(response.text)

        print(f"Hammer: {hid}, Throw:\t{i}, Elapsed Time: {endTime:>.2f}")

    # Average of all throws
    average = totalTime / throws
    print(f"Hammer: {hid}, AVERAGE:\t, Elapsed Time: {average:>.2f}")

    return average


def do_hammer(args):
    ''' Use args tuple to call `hammer` '''
    return hammer(*args)


def main():
    arguments = sys.argv[1:]
    hammers = 1
    throws = 1
    verbose = False
    URL = None

    # Parse command line arguments
    if len(arguments) == 0:
        usage(1)

    while len(arguments) != 0:
        temp = arguments.pop(0)
        if temp == '-h':
            try:
                hammers = int(arguments.pop(0))
            except IndexError:
                usage(1)

        elif temp == '-t':
            try:
                throws = int(arguments.pop(0))
            except IndexError:
                usage(1)
        elif temp == '-v':
            verbose = True
        elif len(arguments) > 0:
            usage(1)
    URL = temp

    # Headers to be used
    arguments = ((URL, throws, verbose, hid) for hid in range(hammers))
    
    # Create pool of workers and perform throws
    with concurrent.futures.ProcessPoolExecutor(hammers) as executor:
        time = executor.map(do_hammer, arguments)

    # Adding all the times to get average
    totalTime = 0
    for i in time:
        totalTime += i

    average = totalTime / hammers
    print(f"TOTAL AVERAGE ELAPSED TIME: {average:>.2f}")


# Main execution
if __name__ == '__main__':
    main()

# vim: set sts=4 sw=4 ts=8 expandtab ft=python:
