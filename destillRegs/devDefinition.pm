use strict;
#Beispiel 
# ========================switch =====================================
# battery powered 1 channel temperature
#  "003D" => {name=>"HM-WDS10-TH-O"           ,st=>'THSensor'          ,cyc=>'00:10' ,rxt=>'c:w:f'  ,lst=>'p'            ,chn=>"",},
# 1 device
# 1 kanal
# 6 peers je kanal erlaubt
#----------------define reglist types-----------------
package usrRegs;
my %listTypes = (
      regDev =>{ intKeyVisib=>1, burstRx=>1, pairCentral=>1,
              },
      regChan       =>{ peerNeedsBurst         =>1,   
		          },
     );
#      -----------assemble device -----------------
my %regList;
$regList{0}={type => "regDev",peers=>1};
$regList{1}={type => "regChan",peers=>6};

sub usr_getHash($){
  my $hn = shift;
  return %regList       if($hn eq "regList"      );
  return %listTypes     if($hn eq "listTypes"       );
}
