    .              dmesg          kill           quit           time           
    [              echo           pkill          printf         timedatectl    
    ?              env            losetup        ps             true           
    alias          exec           ln             pwd            uname          
    unalias        exit           ls             reboot         umount         
    arp            expr           mkdir          rm             unset          
    basename       false          mkfifo         rmdir          uptime         
    break          fdinfo         mkrd           rpmsg          usleep         
    cat            free           mh             rptun          watch          
    cd             memdump        mount          set            xd             
    cp             help           mv             shutdown       wait           
    cmp            hexdump        mw             sleep          
    date           ifconfig       nslookup       source         
    dd             ifdown         pidof          test           
    df             ifup           poweroff       top



free
total       used       free    maxused    maxfree  nused  nfree name
1756540     832740     923800    1000000     871744   1652     35 Umem



ps
PID GROUP PRI POLICY   TYPE    NPX STATE    EVENT     SIGMASK            STACK    USED FILLED    CPU COMMAND
0     0   0 FIFO     Kthread   - Ready              0000000000000000 0003040 0001244  40.9%  85.6% Idle_Task
1     0 224 RR       Kthread   - Waiting  Semaphore 0000000000000000 0004016 0000588  14.6%   0.0% hpwork 0x3400adc4 0x3400ade8
2     0 100 RR       Kthread   - Waiting  Semaphore 0000000000000000 0004016 0000684  17.0%   0.0% lpwork 0x3400ad8c 0x3400adb0
5     5 255 RR       Task      - Waiting  Signal    0000000000000000 0004032 0001260  31.2%   0.0% bes_cmsis_wrapper
6     0 224 RR       Kthread   - Waiting  Semaphore 0000000000000000 0004024 0002236  55.5%   0.7% rptun audio 0x3410ef58
7     5 101 RR       pthread   - Waiting  Semaphore 8000000000000000 0004064 0001508  37.1%   0.0% bes_main 0xc1a7e95 0x34110620