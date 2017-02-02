16 Feb.  Alexey's patch
=======================

                                        zcc/zcs(8k)                 bw_tcp

Local networking.  MTU=3856
---------------------------

2x500MHz PII  2.4.2-pre3+ZC+ANK             56                        59
2x500MHz PII  2.4.2-pre3+ZC                 56                        52
2x500MHz PII  2.4.2-pre3                    56                        56

1x650MHz PIII 2.4.2-pre3+ZC+ANK            110                      140-150
1x650MHz PIII 2.4.2-pre3+ZC                 82                      104-120
1x650MHz PIII 2.4.2-pre3                   111                  161,137,138,138


Local networking.  MTU=16144
----------------------------

2x500MHz PII  2.4.2-pre3+ZC+ANK            67                     63,64,60
2x500MHz PII  2.4.2-pre3+ZC                65                     68,63,66
2x500MHz PII  2.4.2-pre3                   71                        67

1x650MHz PIII 2.4.2-pre3+ZC+ANK           120                  178,168,169,172
1x650MHz PIII 2.4.2-pre3+ZC               110                  226,221,166,124
1x650MHz PIII 2.4.2-pre3                  135                    232,249,210


Local networking.  MTU=16436
----------------------------

2x500MHz PII  2.4.2-pre3+ZC+ANK             67                        63
2x500MHz PII  2.4.2-pre3+ZC                 65                        60
2x500MHz PII  2.4.2-pre3                    67                     57,66,61

1x650MHz PIII 2.4.2-pre3+ZC+ANK            130                      170-180
1x650MHz PIII 2.4.2-pre3+ZC                106                  480,207,245,201
1x650MHz PIII 2.4.2-pre3                   131                      160-170


18 Feb.  ANK-2
==============

Local networking.  MTU=16144 send(8k) Mbytes/sec
------------------------------------------------

2x500MHz PII  2.4.2-pre4+ZC+ANK2+FP       66-68
2x500MHz PII  2.4.2-pre4+ZC+ANK2           69
2x500MHz PII  2.4.2-pre3+ZC+ANK            67
2x500MHz PII  2.4.2-pre4+ZC                65
2x500MHz PII  2.4.2-pre4                   71

1x650MHz PIII 2.4.2-pre4+ZC+ANK2+FP      125-132
1x650MHz PIII 2.4.2-pre4+ZC+ANK2         110-114
1x650MHz PIII 2.4.2-pre4+ZC                78
1x650MHz PIII 2.4.2-pre4                 135-140

3c905C.  send(8k) cyclesoak load
--------------------------------

2x500MHz PII  2.4.2-pre4+ZC+ANK2+FP       18.7%
2x500MHz PII  2.4.2-pre4+ZC+ANK2          18.6%
2x500MHz PII  2.4.2-pre4+ZC               20.2%
2x500MHz PII  2.4.2-pre4                  18.4%

1x650MHz PIII 2.4.2-pre4+ZC+ANK2+FP       27.3%
1x650MHz PIII 2.4.2-pre4+ZC+ANK2          27.2%
1x650MHz PIII 2.4.2-pre4+ZC               28.6%
1x650MHz PIII 2.4.2-pre4                  27.8%


