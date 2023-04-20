# Client-Server-Project
In this client-server project, a client can request a file or a set of files from the server.<br />
The server searches for the file/s in its file directory rooted at its ~ and returns the tar.gz of the
file/files requested to the client (or an appropriate message otherwise).<br />
Multiple clients can connect to the server from different machines and can request file/s as
per the commands listed in section 2<br />
The server, the mirror and the client processes must run on different machines and
must communicate using sockets only.<br /><br />
<strong>Section 1 (Server)</strong><br />
    <ul>
    <li>The server and an identical copy of the server called the mirror [see section 3] must
        both run before any of the client (s) run and both of them must wait for request/s
        from client/s</li><br />
    <li>Upon receiving a connection request from a client, the server forks a child process
        that services the client request exclusively in a function called processclient() and
        (the server) returns to listening to requests from other clients.<br />
        <ul>
        <li>The processclient() function enters an infinite loop waiting for the client to
            send a command</li><br />
        <li>Upon the receipt of a command from the client, processclient() performs the
            action required to process the command as per the requirements listed in
            section 2 and returns the result to the client</li><br />
        </ul>
    </li>
    <li>Upon the receipt of quit from the client, processclient() exits.</li><br />
    <li>Note: for each client request, the server must fork a separate process with the
        processclient() function to service the request and then go back to listening to
        requests from other clients</li><br />
    </ul><br />
<strong>Section 2 (Client)</strong><br />
    The client process runs an infinite loop waiting for the user to enter one of the commands.<br />
    Note: The commands are not Linux commands and are defined(in this project) to denote the
    action to be performed by the server.<br />
    Once the command is entered, the client verifies the syntax of the command and if it is okay,
    sends the command to the server, else it prints an appropriate error message.<br /><br />
    <strong>List of Client Commands:</strong><br />
    <ul>
        <li>findfile filename<br />
            If the file filename is found in its file directory tree rooted at ~, the server must
            return the filename, size(in bytes), and date created to the client and the
            client prints the received information on its terminal.<br />
            Note: if the file with the same name exists in multiple folders in the
            directory tree rooted at ~, the server sends information pertaining to
            the first successful search/match of filename<br />
            Else the client prints “File not found”<br />
            Ex: C$ findfile sample.txt</li><br />
        <li>sgetfiles size1 size2 <-u><br />
            The server must return to the client temp.tar.gz that contains all the files in
            the directory tree rooted at its ~ whose file-size in bytes is >=size1 and <=size2<br />
            size1 < = size2 (size1>= 0 and size2>=0)<br />
            -u unzip temp.tar.gz in the pwd of the client<br />
            Ex: C$ sgetfiles 1240 12450 -u</li><br />
        <li>dgetfiles date1 date2 <-u><br />
            The server must return to the client temp.tar.gz that contains all the files in the
            directory tree rooted at ~ whose date of creation is >=date1 and <=date2
            (date1<=date2)<br />
            -u unzip temp.tar.gz in the pwd of the client<br />
            Ex: C$ dgetfiles 2023-01-16 2023-03-04 -u</li><br />
        <li>getfiles file1 file2 file3 file4 file5 file6 <-u ><br />
            The server must search the files (file 1 ..up to file6) in its directory tree rooted
            at ~ and return temp.tar.gz that contains at least one (or more of the listed
            files) if they are present<br />
            If none of the files are present, the server sends “No file found” to the client
            (which is then printed on the client terminal by the client)<br />
            -u unzip temp.tar.gz in the pwd of the client<br />
            Ex: C$ getfiles new.txt ex1.c ex4.pdf</li><br />
        <li>gettargz <extension list> <-u> //up to 6 different file types<br />
            the server must return temp.tar.gz that contains all the files in its directory tree
            rooted at ~ belonging to the file type/s listed in the extension list, else the
            server sends the message “No file found” to the client (which is printed on the
            client terminal by the client)<br />
            -u unzip temp.tar.gz in the pwd of client<br />
            The extension list must have at least one file type and can have up to six
            different file types<br />
            Ex: C$ gettargz c txt pdf</li><br />
        <li>quit<br />
            The command is transferred to the server and the client process is terminated</li><br />
    </ul>
    Note:<br />
    It is the responsibility of the client process to verify the syntax of the command
    entered by the user (as per the rules in Section 3) before processing it.<br />
    Appropriate messages must be printed when the syntax of the command is
    incorrect.<br />
    It is the responsibility of the client process to unzip the tar files whenever the option
    is specified.<br /><br />
    <strong>Section 3 Alternating Between the Server and the Mirror</strong><br />
    The server and the mirror (the server’s copy possibly with a few
    additions/changes) are to run on two different machines/terminals.<br />
    The first 4 client connections are to be handled by the server.<br />
    The next 4 client connections are to be handled by the mirror.<br />
    The remaining client connections are to be handled by the server and the
    mirror in an alternating manner- (ex: connection 9 is to be handled by the
    server, connection 10 by the mirror, and so on)<br />
    
<strong>TO RUN THE CODE ON MULTIPLE MACHINES IN SAME NETWORK</strong>
    <ol>
        <li>Start running the code on 1 machine - Server machine</li>
        <li>Get the server IP address using the following on Server machine -
        <ul>
            <li>"ipconfig" from Windows Terminal or Windows Powershell.</li>
            <li>"ifconfig" from MAC Terminal.</li>
        </ul>
        </li>
        <li>In the client.c file to be run on te client machine, enter the fetched server IP address in the SERVER_IP_ADDRESS definition string (around line 20)</li>
        <li>Try executing client code on the client machine and check if connection is successful or in wait state</li>
        <li>If execution of client code goes to wait state, consider doing one or all of the following and retry. After all of them, execution WILL WORK-
        <ul>
            <li>Do port forwarding if your server code is working in WSL. Write the command: "netsh interface portproxy add v4tov4 listenport=8080 listenaddress=<IPV4 address fetched from ipconfig> connectport=8080 connectaddress=$($(wsl hostname -I).Trim());" in the Windows Powershell(running as Administrator) </li>
            <li>Turn off the active Firewalls, if any</li>
        </ul>
        </li>
    </ol>
