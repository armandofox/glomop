#!/usr/sww/bin/perl5

@dcns = (
"844729660.log.univ.gz", "844744060.log.univ.gz", "844758461.log.univ.gz", "844772861.log.univ.gz", 
"844787261.log.univ.gz", "844801661.log.univ.gz", "844816061.log.univ.gz", "844830461.log.univ.gz", 
"844844861.log.univ.gz", "844859261.log.univ.gz", "844873661.log.univ.gz", "844888061.log.univ.gz", 
"844902461.log.univ.gz", "844916861.log.univ.gz", "844931262.log.univ.gz", "844945662.log.univ.gz", 
"844960062.log.univ.gz", "844974462.log.univ.gz", "844988862.log.univ.gz", "845003262.log.univ.gz", 
"845017662.log.univ.gz", "845032062.log.univ.gz", "845046462.log.univ.gz", "845060862.log.univ.gz", 
"845075262.log.univ.gz", "845089662.log.univ.gz", "845104062.log.univ.gz", "845118462.log.univ.gz", 
"845132862.log.univ.gz", "845147262.log.univ.gz", "845161662.log.univ.gz", "845176062.log.univ.gz", 
"845190463.log.univ.gz", "845204863.log.univ.gz", "845219263.log.univ.gz", "845233663.log.univ.gz", 
"845248063.log.univ.gz", "845262463.log.univ.gz", "845276863.log.univ.gz", "845305663.log.univ.gz", 
"845320063.log.univ.gz", "845334464.log.univ.gz", "845348864.log.univ.gz", "845363264.log.univ.gz", 
"845377664.log.univ.gz", "845392064.log.univ.gz", "845406464.log.univ.gz", "845420864.log.univ.gz", 
"845435264.log.univ.gz", "845449664.log.univ.gz", "845464065.log.univ.gz", "845478465.log.univ.gz", 
"845492865.log.univ.gz", "845507265.log.univ.gz", "845521665.log.univ.gz", "845536065.log.univ.gz", 
"845550465.log.univ.gz", "845564865.log.univ.gz", "845579265.log.univ.gz", "845593667.log.univ.gz", 
"845600169.log.univ.gz", "845600338.log.univ.gz", "845614738.log.univ.gz", "845629138.log.univ.gz", 
"845643538.log.univ.gz", "845657938.log.univ.gz", "845672338.log.univ.gz", "845686738.log.univ.gz", 
"845701138.log.univ.gz", "845715538.log.univ.gz", "845729938.log.univ.gz", "845744338.log.univ.gz", 
"845758739.log.univ.gz", "845759365.log.univ.gz", "845766438.log.univ.gz", "845773139.log.univ.gz", 
"845787539.log.univ.gz", "845801939.log.univ.gz", "845816339.log.univ.gz", "845830739.log.univ.gz", 
"845845139.log.univ.gz", "845859539.log.univ.gz", "845873939.log.univ.gz", "845888339.log.univ.gz", 
"845902739.log.univ.gz", "845917139.log.univ.gz", "845931540.log.univ.gz", "845945940.log.univ.gz", 
"845949102.log.univ.gz", "845960340.log.univ.gz", "845974740.log.univ.gz", "845989140.log.univ.gz", 
"846003540.log.univ.gz", "846017940.log.univ.gz", "846032340.log.univ.gz", "846046740.log.univ.gz", 
"846061140.log.univ.gz", "846075540.log.univ.gz", "846089940.log.univ.gz", "846104340.log.univ.gz", 
"846118740.log.univ.gz", "846133141.log.univ.gz", "846146603.log.univ.gz", "846147541.log.univ.gz", 
"846161941.log.univ.gz", "846176341.log.univ.gz", "846190741.log.univ.gz", "846205141.log.univ.gz", 
"846219541.log.univ.gz", "846233941.log.univ.gz", "846248341.log.univ.gz", "846262741.log.univ.gz"
	 );

@dcns2 = (
"846269483.log.univ.gz", "846271135.log.univ.gz", "846285535.log.univ.gz", "846299935.log.univ.gz", 
"846314335.log.univ.gz", "846328736.log.univ.gz", "846343136.log.univ.gz", "846357536.log.univ.gz", 
"846371936.log.univ.gz", "846386336.log.univ.gz", "846400736.log.univ.gz", "846415136.log.univ.gz", 
"846429536.log.univ.gz", "846443936.log.univ.gz", "846458336.log.univ.gz", "846472737.log.univ.gz", 
"846487137.log.univ.gz", "846501537.log.univ.gz", "846515937.log.univ.gz", "846530337.log.univ.gz", 
"846544737.log.univ.gz", "846556186.log.univ.gz", "846559137.log.univ.gz", "846573537.log.univ.gz", 
"846587937.log.univ.gz", "846602337.log.univ.gz", "846616737.log.univ.gz", "846631137.log.univ.gz", 
"846645537.log.univ.gz", "846659938.log.univ.gz", "846668518.log.univ.gz", "846674338.log.univ.gz", 
"846688738.log.univ.gz", "846703138.log.univ.gz", "846717538.log.univ.gz", "846731938.log.univ.gz", 
"846746338.log.univ.gz", "846760738.log.univ.gz", "846775138.log.univ.gz", "846789538.log.univ.gz", 
"846803938.log.univ.gz", "846818338.log.univ.gz", "846832738.log.univ.gz", "846847138.log.univ.gz", 
"846861539.log.univ.gz", "846875939.log.univ.gz"
);

@dcns3 = (
"846890339.log.univ.gz", "846895617.log.univ.gz", "846910017.log.univ.gz", "846924417.log.univ.gz", 
"846938817.log.univ.gz", "846953217.log.univ.gz", "846967617.log.univ.gz", "846982017.log.univ.gz", 
"846996417.log.univ.gz", "847010817.log.univ.gz", "847025217.log.univ.gz", "847039618.log.univ.gz", 
"847054018.log.univ.gz", "847068418.log.univ.gz", "847075295.log.univ.gz", "847082818.log.univ.gz", 
"847097218.log.univ.gz", "847111618.log.univ.gz", "847126018.log.univ.gz", "847140418.log.univ.gz", 
"847154818.log.univ.gz", "847169218.log.univ.gz", "847183618.log.univ.gz", "847198018.log.univ.gz", 
"847212418.log.univ.gz", "847226819.log.univ.gz", "847241219.log.univ.gz", "847255619.log.univ.gz", 
"847270019.log.univ.gz", "847284419.log.univ.gz", "847298819.log.univ.gz", "847313219.log.univ.gz", 
"847327619.log.univ.gz", "847342019.log.univ.gz", "847356420.log.univ.gz", "847370820.log.univ.gz", 
"847385220.log.univ.gz", "847399620.log.univ.gz", "847414020.log.univ.gz", "847428420.log.univ.gz", 
"847442820.log.univ.gz", "847457220.log.univ.gz", "847471620.log.univ.gz", "847486020.log.univ.gz", 
"847500420.log.univ.gz", "847514820.log.univ.gz", "847529221.log.univ.gz", "847543621.log.univ.gz", 
"847558021.log.univ.gz", "847572421.log.univ.gz", "847586821.log.univ.gz", "847601221.log.univ.gz", 
"847615621.log.univ.gz", "847630021.log.univ.gz", "847644421.log.univ.gz", "847658821.log.univ.gz", 
"847673221.log.univ.gz", "847687622.log.univ.gz", "847702022.log.univ.gz", "847716422.log.univ.gz", 
"847730822.log.univ.gz", "847741777.log.univ.gz", "847745222.log.univ.gz", "847759622.log.univ.gz", 
"847774022.log.univ.gz", "847788422.log.univ.gz", "847802822.log.univ.gz", "847817223.log.univ.gz", 
"847831623.log.univ.gz", "847846023.log.univ.gz", "847860423.log.univ.gz", "847874823.log.univ.gz", 
"847889223.log.univ.gz", "847903623.log.univ.gz", "847918023.log.univ.gz", "847932423.log.univ.gz", 
"847946824.log.univ.gz", "847961224.log.univ.gz", "847975624.log.univ.gz", "847990024.log.univ.gz", 
"848004424.log.univ.gz", "848018824.log.univ.gz", "848033224.log.univ.gz", "848047624.log.univ.gz", 
"848062024.log.univ.gz", "848076424.log.univ.gz", "848090824.log.univ.gz", "848105224.log.univ.gz", 
"848119625.log.univ.gz", "848134025.log.univ.gz", "848148425.log.univ.gz", "848162825.log.univ.gz", 
"848164972.log.univ.gz", "848172103.log.univ.gz", "848172225.log.univ.gz", "848172245.log.univ.gz", 
"848177225.log.univ.gz", "848191625.log.univ.gz", "848206025.log.univ.gz", "848220425.log.univ.gz", 
"848234825.log.univ.gz", "848249225.log.univ.gz", "848263625.log.univ.gz", "848278026.log.univ.gz", 
"848292426.log.univ.gz", "848306826.log.univ.gz", "848321226.log.univ.gz", "848335626.log.univ.gz", 
"848350026.log.univ.gz", "848353668.log.univ.gz", "848353923.log.univ.gz", "848368323.log.univ.gz", 
"848409356.log.univ.gz", "848409417.log.univ.gz" );

#foreach $nextfile (@dcns) {
#    &do_count($nextfile, "/home/gribble/clienttraces/dcns", "1");
#}

#foreach $nextfile (@dcns2) {
#    &do_count($nextfile, "/home/gribble/clienttraces/dcns2", "2");
#}

foreach $nextfile (@dcns3) {
    &do_count($nextfile, "/tmp/clienttraces/dcns3", "3");
}

1;

sub do_count {
    my $file = shift(@_);
    my $remotepath = shift(@_);
    my $vers = shift(@_);
    my ($countcmd, $countnewcmd, $file2, $numlines, $numnewlines, $shot);

    $shot = "/home/gribble/working/tcsproxy/playback/count_parallel";
    $countcmd = "zcat ${remotepath}/${file} | $shot | gzip >> parallel_counts.txt.gz";
    print "about to do $countcmd\n";
    system("$countcmd");
}
