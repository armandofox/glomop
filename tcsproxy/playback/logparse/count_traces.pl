#!/usr/sww/bin/perl5

@dcns = (
"844729660.log.gz", "844744060.log.gz", "844758461.log.gz", "844772861.log.gz", 
"844787261.log.gz", "844801661.log.gz", "844816061.log.gz", "844830461.log.gz", 
"844844861.log.gz", "844859261.log.gz", "844873661.log.gz", "844888061.log.gz", 
"844902461.log.gz", "844916861.log.gz", "844931262.log.gz", "844945662.log.gz", 
"844960062.log.gz", "844974462.log.gz", "844988862.log.gz", "845003262.log.gz", 
"845017662.log.gz", "845032062.log.gz", "845046462.log.gz", "845060862.log.gz", 
"845075262.log.gz", "845089662.log.gz", "845104062.log.gz", "845118462.log.gz", 
"845132862.log.gz", "845147262.log.gz", "845161662.log.gz", "845176062.log.gz", 
"845190463.log.gz", "845204863.log.gz", "845219263.log.gz", "845233663.log.gz", 
"845248063.log.gz", "845262463.log.gz", "845276863.log.gz", "845305663.log.gz", 
"845320063.log.gz", "845334464.log.gz", "845348864.log.gz", "845363264.log.gz", 
"845377664.log.gz", "845392064.log.gz", "845406464.log.gz", "845420864.log.gz", 
"845435264.log.gz", "845449664.log.gz", "845464065.log.gz", "845478465.log.gz", 
"845492865.log.gz", "845507265.log.gz", "845521665.log.gz", "845536065.log.gz", 
"845550465.log.gz", "845564865.log.gz", "845579265.log.gz", "845593667.log.gz", 
"845600169.log.gz", "845600338.log.gz", "845614738.log.gz", "845629138.log.gz", 
"845643538.log.gz", "845657938.log.gz", "845672338.log.gz", "845686738.log.gz", 
"845701138.log.gz", "845715538.log.gz", "845729938.log.gz", "845744338.log.gz", 
"845758739.log.gz", "845759365.log.gz", "845766438.log.gz", "845773139.log.gz", 
"845787539.log.gz", "845801939.log.gz", "845816339.log.gz", "845830739.log.gz", 
"845845139.log.gz", "845859539.log.gz", "845873939.log.gz", "845888339.log.gz", 
"845902739.log.gz", "845917139.log.gz", "845931540.log.gz", "845945940.log.gz", 
"845949102.log.gz", "845960340.log.gz", "845974740.log.gz", "845989140.log.gz", 
"846003540.log.gz", "846017940.log.gz", "846032340.log.gz", "846046740.log.gz", 
"846061140.log.gz", "846075540.log.gz", "846089940.log.gz", "846104340.log.gz", 
"846118740.log.gz", "846133141.log.gz", "846146603.log.gz", "846147541.log.gz", 
"846161941.log.gz", "846176341.log.gz", "846190741.log.gz", "846205141.log.gz", 
"846219541.log.gz", "846233941.log.gz", "846248341.log.gz", "846262741.log.gz"
	 );

@dcns2 = (
"846269483.log.gz", "846271135.log.gz", "846285535.log.gz", "846299935.log.gz", 
"846314335.log.gz", "846328736.log.gz", "846343136.log.gz", "846357536.log.gz", 
"846371936.log.gz", "846386336.log.gz", "846400736.log.gz", "846415136.log.gz", 
"846429536.log.gz", "846443936.log.gz", "846458336.log.gz", "846472737.log.gz", 
"846487137.log.gz", "846501537.log.gz", "846515937.log.gz", "846530337.log.gz", 
"846544737.log.gz", "846556186.log.gz", "846559137.log.gz", "846573537.log.gz", 
"846587937.log.gz", "846602337.log.gz", "846616737.log.gz", "846631137.log.gz", 
"846645537.log.gz", "846659938.log.gz", "846668518.log.gz", "846674338.log.gz", 
"846688738.log.gz", "846703138.log.gz", "846717538.log.gz", "846731938.log.gz", 
"846746338.log.gz", "846760738.log.gz", "846775138.log.gz", "846789538.log.gz", 
"846803938.log.gz", "846818338.log.gz", "846832738.log.gz", "846847138.log.gz", 
"846861539.log.gz", "846875939.log.gz"
);

@dcns3 = (
"846890339.log.gz", "846895617.log.gz", "846910017.log.gz", "846924417.log.gz", 
"846938817.log.gz", "846953217.log.gz", "846967617.log.gz", "846982017.log.gz", 
"846996417.log.gz", "847010817.log.gz", "847025217.log.gz", "847039618.log.gz", 
"847054018.log.gz", "847068418.log.gz", "847075295.log.gz", "847082818.log.gz", 
"847097218.log.gz", "847111618.log.gz", "847126018.log.gz", "847140418.log.gz", 
"847154818.log.gz", "847169218.log.gz", "847183618.log.gz", "847198018.log.gz", 
"847212418.log.gz", "847226819.log.gz", "847241219.log.gz", "847255619.log.gz", 
"847270019.log.gz", "847284419.log.gz", "847298819.log.gz", "847313219.log.gz", 
"847327619.log.gz", "847342019.log.gz", "847356420.log.gz", "847370820.log.gz", 
"847385220.log.gz", "847399620.log.gz", "847414020.log.gz", "847428420.log.gz", 
"847442820.log.gz", "847457220.log.gz", "847471620.log.gz", "847486020.log.gz", 
"847500420.log.gz", "847514820.log.gz", "847529221.log.gz", "847543621.log.gz", 
"847558021.log.gz", "847572421.log.gz", "847586821.log.gz", "847601221.log.gz", 
"847615621.log.gz", "847630021.log.gz", "847644421.log.gz", "847658821.log.gz", 
"847673221.log.gz", "847687622.log.gz", "847702022.log.gz", "847716422.log.gz", 
"847730822.log.gz", "847741777.log.gz", "847745222.log.gz", "847759622.log.gz", 
"847774022.log.gz", "847788422.log.gz", "847802822.log.gz", "847817223.log.gz", 
"847831623.log.gz", "847846023.log.gz", "847860423.log.gz", "847874823.log.gz", 
"847889223.log.gz", "847903623.log.gz", "847918023.log.gz", "847932423.log.gz", 
"847946824.log.gz", "847961224.log.gz", "847975624.log.gz", "847990024.log.gz", 
"848004424.log.gz", "848018824.log.gz", "848033224.log.gz", "848047624.log.gz", 
"848062024.log.gz", "848076424.log.gz", "848090824.log.gz", "848105224.log.gz", 
"848119625.log.gz", "848134025.log.gz", "848148425.log.gz", "848162825.log.gz", 
"848164972.log.gz", "848172103.log.gz", "848172225.log.gz", "848172245.log.gz", 
"848177225.log.gz", "848191625.log.gz", "848206025.log.gz", "848220425.log.gz", 
"848234825.log.gz", "848249225.log.gz", "848263625.log.gz", "848278026.log.gz", 
"848292426.log.gz", "848306826.log.gz", "848321226.log.gz", "848335626.log.gz", 
"848350026.log.gz", "848353668.log.gz", "848353923.log.gz", "848368323.log.gz", 
"848409356.log.gz", "848409417.log.gz"	  );

&init_count();
foreach $nextfile (@dcns) {
    &do_count($nextfile, "/disks/barad-dur/sandbox1/gribble/clienttraces/dcns", "1");
}
&dump_count(@dcns);

&init_count();
foreach $nextfile (@dcns2) {
    &do_count($nextfile, "/disks/barad-dur/sandbox1/gribble/clienttraces/dcns2", "2");
}
&dump_count(@dcns2);

&init_count();
foreach $nextfile (@dcns3) {
    &do_count($nextfile, "/disks/barad-dur/sandbox1/gribble/clienttraces/dcns3", "3");
}
&dump_count(@dcns2);

1;

sub do_count {
    my $file = shift(@_);
    my $remotepath = shift(@_);
    my $vers = shift(@_);
    my ($countcmd, $countnewcmd, $file2, $numlines, $numnewlines, $shot);

    $file2 = $file;  chop($file2);  chop($file2); chop($file2);
    $file2 = "${file2}.univ.gz";
    $shot = "/home/tomorrow/b/grad/gribble/working/tcsproxy/playback/showtrace";
    $countcmd = "gzcat ${remotepath}/${file} | wc -l |";
    $countnewcmd = "gzcat ${remotepath}/${file2} | $shot | wc -l |";

    if (open(CONTCMD, $countcmd)) {
	$numlines = <CONTCMD>;
	close(CONTCMD);
	chomp($numlines);
    } else {
	print "Couldn't do $countcmd..\n";
    }
    if (open(CONTNEWCMD, $countnewcmd)) {
	$numnewlines = <CONTNEWCMD>;
	close(CONTNEWCMD);
	chomp($numnewlines);
    } else {
	print "Couldn't do $countnewcmd...\n";
    }
    $counts{$file} = $numlines;
    $counts{$file2} = $numnewlines;
}

sub dump_count {
    my ($nextfile, $nextnewfile);

    foreach $nextfile (@_) {
	$nextnewfile = $nextfile;  chop($nextnewfile);  chop($nextnewfile); chop($nextnewfile);
	$nextnewfile = "${nextnewfile}.univ.gz";
	print "$nextfile:  Old $counts{$nextfile},  New $counts{$nextnewfile}\n";
    }
}

sub init_count {
    undef %counts;
}
