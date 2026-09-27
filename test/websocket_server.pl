#!/usr/bin/perl

# Script state:
#			JSON parser
#			TODO list
#			Box opening simulation ($dooropentime - how long are doors open
#									$ambientonoverhang - how long is ambient on after door slosing
#									)
#			PIN management
#			Watchdog
#			BoxTime
#			Parameters		port=<listening port> [8080]
#							dooropentime=<time doors are kept open> [10s]
#							ambientoverhang=<how long is ambient on after closing door> [3s]
#							watchdogperiod=<how often ping is sent to clients> [20s]
#							deadtimeout=<when the connection is deemed dead> [25s]
#							pintable=<file with initial pins>		pinid,pinname,pinvalue,amount,not_before,not_after
#							codetable=<file with initial codes>		codeid,codename,codevalue,not_before,not_after
#			Status
#




use strict;
use warnings;
use Net::WebSocket::Server;
use JSON::MaybeXS;
use Data::Dumper;

my $configfile;
my $codefile;
my $filehandle;	
my $args = join (' ', @ARGV);

my $port = 8080;									#ws listening port
my $tickperiod = 1;									#internal loop interval
my $watchdogperiod = 20;							#watchdog interval
my $deadtimeout = 25;
my %last_pong;										#last seen times for clients
my @todolist = ();									#future actions to be performed
my $dooropentime = 10;								#how long should be doors opened
my $ambientonoverhang = 3;							#how long is ambient on after closing doors
my $lastaction = "";								#last action result
my $lastpin = 0;										#first available pinId
my $lastcode = 0;
my $last_command_status = 'wat';
my $door_state_closed = 'yes';
my $door_state_open = 'no';
my $ambient_state_off = 'yes';
my $ambient_state_on = 'no';
my $prevstat;
my $prevping;
my $globalnow;
my $statuschange = 0;


my $json = JSON::MaybeXS->new(utf8 => 1, pretty => 0);
my $ws;

$globalnow = $prevstat = $prevping = time();

foreach my $argument (@ARGV) {
	if ( $argument =~ /port=(.+)/ ){
		$port = $1;
		$port =~ s/'//g;
	#												print "StartTime--$starttime\n";
	}
	if ( $argument =~ /dooropentime=(.+)/ ){
		$dooropentime = $1;
		$dooropentime =~ s/'//g;
	#												print "StartDate--$startdate\n";
	}
	if ( $argument =~ /ambientoverhang=(.+)/ ){
		$ambientonoverhang = $1;
		$ambientonoverhang =~ s/'//g;
	#												print "StartDate--$startdate\n";
	}
	if ( $argument =~ /watchdogperiod=(.+)/ ){
		$watchdogperiod = $1;
		$watchdogperiod =~ s/'//g;
	#												print "StartDate--$startdate\n";
	}
	if ( $argument =~ /deadtimeout=(.+)/ ){
		$deadtimeout = $1;
		$deadtimeout =~ s/'//g;
	#												print "StartDate--$startdate\n";
	}
	if ( $argument =~ /pintable=(.+)/ ){
		$configfile = $1;
		$configfile =~ s/'//g;
	#												print "StartDate--$startdate\n";
	}
		if ( $argument =~ /codetable=(.+)/ ){
		$codefile = $1;
		$codefile =~ s/'//g;
	#												print "StartDate--$startdate\n";
	}

}

my $statemessage_src = sub {
    return {
        _timestamp_ => time(),
        _command_ => 'c_content',
        data => {
            lastresult => $last_command_status,
        },
    }
};

my $door_src =  sub {
	return {
		_timestamp_ => time(),
		_command_ => 'c_visibility',
		data => {
			door_state_open => $door_state_open,
			door_state_closed => $door_state_closed,
		},
	}
};

my $ambient_src = sub {
	return {
		_timestamp_ => time(),
		_command_ => 'c_visibility',
		data => {
			ambient_state_off => $ambient_state_off,
			ambient_state_on => $ambient_state_on,
		},
	}
};


#my $pintable_src = sub {
#	return {
#		_timestamp_ => time(),
#		_command_ => 'c_content',
#		data => {
#			pinrows => getPins(),
#		},
#	}
#};

#my $codetable_src = sub {
#	return {
#		_timestamp_ => time(),
#		_command_ => 'c_content',
#		data => {
#			coderows => getCodes(),
#		},
#	}
#};

my $codetable_src = sub {
	return {
		_timestamp_ => time(),
		_command_ => 'c_setcred',
		data => {
		credType => 'ctCode',
		rowCount => getCredsCount('code'),
		rows => getCreds('code'),
		},
	}
};

my $pintable_src = sub {
	return {
		_timestamp_ => time(),
		_command_ => 'c_setcred',
		data => {
		credType => 'ctPin',
		rowCount => getCredsCount('pin'),
		rows => getCreds('pin'),
		},
	}
};

my $openboxmessage_src =  sub {
	return {
		_timestamp_ => $globalnow,
		_command_ => 'c_visibility',
		data => {
			door_state_open => 'yes',
			door_state_closed => 'no',
			ambient_state_off => 'no',
			ambient_state_on => 'yes',
		},
	}
};

my @knownPins = ();
my @knownCodes = ();

sub read_config  {
	my $inihandler = shift;
#	my $confstructptr = shift;

	while (<$inihandler>) {
		chomp;
		s/#.*//;
#		s/\s//g;
		
		my ($pinid, $pinname, $pinvalue, $notbefore, $notafter, $amount) = split(/,/);
		
		push ( @knownPins, [$pinid, $pinname, $pinvalue, $notbefore, $notafter, $amount] ); 
	}
#	print Dumper @knownPins;
	return 1;
}

sub read_code  {
	my $inihandler = shift;
#	my $confstructptr = shift;

	while (<$inihandler>) {
		chomp;
		s/#.*//;
#		s/\s//g;
		
		my ($codeid, $codename, $codevalue, $notbefore, $notafter) = split(/,/);
		
		push ( @knownCodes, [$codeid, $codename, $codevalue, $notbefore, $notafter] ); 
	}
#	print Dumper @knownPins;
	return 1;
}
# no more needed
#sub getPins  {									# compute innerHTML for pintable tbody
#	my $pintable = "";
#	for(my $idx = 0; $idx < scalar @knownPins; $idx++){
#		print "\nAssigning: ( $idx )\n";
#		print Dumper @knownPins[$idx];
#		my ($id, $name, $pinvalue, $notbefore, $notafter, $count) = @{$knownPins[$idx]}[0..5];
#		my $row = "<tr><td>$id</td><td>$name</td><td>$pinvalue</td><td>$notbefore</td><td>$notafter</td><td>$count</td></tr>";  
#		$pintable =  $pintable . $row;
#	}
#	return $pintable;
#}        

# no more needed
#sub getCodes  {									# compute innerHTML for codetable tbody
#	my $codetable = "";
#	for(my $idx = 0; $idx < scalar @knownCodes; $idx++){
#		print "\nAssigning: ( $idx )\n";
#		print Dumper @knownPins[$idx];
#		my ($id, $name, $pinvalue, $notbefore, $notafter) = @{$knownCodes[$idx]}[0..4];
#		my $row = "<tr><td>$id</td><td>$name</td><td>$pinvalue</td><td>$notbefore</td><td>$notafter</td></tr>";  
#		$codetable =  $codetable . $row;
#	}
#	return $codetable;
#}        

sub getCredsCount {
	my $codeType = shift;
	if( $codeType eq 'code' ){
		return scalar @knownCodes;
	}
		if( $codeType eq 'pin' ){
		return scalar @knownPins;
	}
	return 0;

}

sub getCreds {
	my $codeType = shift;
	if( $codeType eq 'code' ){
		return getCodesJson();
	}
	if( $codeType eq 'pin' ){
		return getPinsJson();
	}
	return 0;

}


sub getCodesJson  {									# compute Json source data for codetable tbody
	my %rows;
	my @cols = qw(codeid codename codevalue codefrom codeto);
	@rows{@cols} = map { my $i = $_; [ map { $_->[$i] } @knownCodes ] } 0..$#cols;	
	return \%rows;
}        

sub getPinsJson  {									# compute Json source data for codetable tbody
	my %rows;
	my @cols = qw(pinid pinname pinvalue datefrom dateto amount);
	@rows{@cols} = map { my $i = $_; [ map { $_->[$i] } @knownPins ] } 0..$#cols;	
	return \%rows;
}        

sub getlastpid {									# first available pin Id
	for ( my $idx = 0; $idx < scalar @knownPins; $idx++ ) {
		if ( $knownPins[$idx][0] > $lastpin ) {
			$lastpin = $knownPins[$idx][0];
		}
	}
	$lastpin++;
	return $lastpin;
}

sub getlastcode {									# first available pin Id
	for ( my $idx = 0; $idx < scalar @knownCodes; $idx++ ) {
		if ( $knownCodes[$idx][0] > $lastcode ) {
			$lastcode = $knownCodes[$idx][0];
		}
	}
	$lastcode++;
	return $lastcode;
}


sub set_code {
	my $rmsg = shift;
	my $conn = shift;
	my $codecommand = $$rmsg{'data'}{'clicked'};
	my $codeid = 0;
	if ( defined $$rmsg{'data'}{'codeid'} ) {
		$codeid = $$rmsg{'data'}{'codeid'} + 0;
	}
	if ( $codecommand eq 'deletebutton' ) {
		my $index = 0;
		$index++ until $knownCodes[$index][0] eq $codeid;
		print "Deleting code $codeid / $index\n";
		splice(@knownCodes, $index, 1);
#		print Dumper @knownPins;
		my $codetable = $json->encode( $codetable_src->() );
		print"Sending: $codetable\n";
		foreach($ws->connections()){
			$_->send_utf8($codetable);
		}
		$last_command_status = "Code $codeid / $$rmsg{'data'}{'codename'} deleted.";
		
		my $mstatemessage = $json->encode( $statemessage_src->() );
		print"Sending: $mstatemessage\n";
		$conn->send_utf8($mstatemessage);
	}
	if ( $codecommand eq 'savebutton' ) {
		if ( $$rmsg{'data'}{'codefrom'} eq "" ) {
			$$rmsg{'data'}{'codefrom'} = "2025-01-01";
		}
		if ( $$rmsg{'data'}{'codeto'} eq "" ) {
			$$rmsg{'data'}{'codeto'} = "2035-12-31";
		}
		unless ( $codeid ) {
			$codeid = $lastcode++;
			print "Adding new Code: $codeid / $$rmsg{'data'}{'codename'} \n";
			push ( @knownCodes, [$codeid, $$rmsg{'data'}{'codename'}, $$rmsg{'data'}{'codevalue'}, $$rmsg{'data'}{'codefrom'}, $$rmsg{'data'}{'codeto'}]);
		} else {
			for ( my $index = 0; $index < scalar @knownCodes; $index++ ) {
				if ( $knownCodes[$index][0] eq $codeid ) {
					print "Updating code $index / $codeid / $$rmsg{'data'}{'codename'} \n";
					$knownCodes[$index][3] = $$rmsg{'data'}{'codefrom'};
					$knownCodes[$index][4] = $$rmsg{'data'}{'codeto'};
				}
			}
		}
#		print Dumper @knownPins;
		my $codetable = $json->encode( $codetable_src->() );
		print"Sending: $codetable\n";
		foreach($ws->connections()){
			$_->send_utf8($codetable);
		}
		$last_command_status = "Code $codeid / $$rmsg{'data'}{'codename'} saved.";
		print Dumper $statemessage_src;
		my $mstatemessage = $json->encode( $statemessage_src->() );
		print"Sending: $mstatemessage\n";
		$conn->send_utf8($mstatemessage);
	}
}


sub set_pin {
	my $rmsg = shift;
	my $conn = shift;
	my $pincommand = $$rmsg{'data'}{'clicked'};
	my $pinid = 0;
	if ( defined $$rmsg{'data'}{'pinid'} ) {
		$pinid = $$rmsg{'data'}{'pinid'} + 0;
	}
	if ( $pincommand eq 'deletebutton' ) {
		my $index = 0;
		$index++ until $knownPins[$index][0] eq $pinid;
		print "Deleting pin $pinid / $index\n";
		splice(@knownPins, $index, 1);
#		print Dumper @knownPins;
		my $pintable = $json->encode( $pintable_src->() );
		print"Sending: $pintable\n";
		foreach($ws->connections()){
			$_->send_utf8($pintable);
		}
		$last_command_status = "PIN $pinid / $$rmsg{'data'}{'pinname'} deleted.";
		
		my $mstatemessage = $json->encode( $statemessage_src->() );
		print"Sending: $mstatemessage\n";
		$conn->send_utf8($mstatemessage);
	}
	if ( $pincommand eq 'savebutton' ) {
		if ( $$rmsg{'data'}{'datefrom'} eq "" ) {
			$$rmsg{'data'}{'datefrom'} = "2025-01-01";
		}
		if ( $$rmsg{'data'}{'dateto'} eq "" ) {
			$$rmsg{'data'}{'dateto'} = "2035-12-31";
		}
		unless ( $pinid ) {
			$pinid = $lastpin++;
			print "Adding new PIN: $pinid / $$rmsg{'data'}{'pinname'} \n";
			push ( @knownPins, [$pinid, $$rmsg{'data'}{'pinname'}, $$rmsg{'data'}{'pinvalue'}, $$rmsg{'data'}{'datefrom'}, $$rmsg{'data'}{'dateto'}, $$rmsg{'data'}{'amount'}]);
		} else {
			for ( my $index = 0; $index < scalar @knownPins; $index++ ) {
				if ( $knownPins[$index][0] eq $pinid ) {
					print "Updating pin $index / $pinid / $$rmsg{'data'}{'pinname'} \n";
					$knownPins[$index][3] = $$rmsg{'data'}{'datefrom'};
					$knownPins[$index][4] = $$rmsg{'data'}{'dateto'};
					$knownPins[$index][5] = $$rmsg{'data'}{'amount'};
				}
			}
		}
#		print Dumper @knownPins;
		my $pintable = $json->encode( $pintable_src->() );
		print"Sending: $pintable\n";
		foreach($ws->connections()){
			$_->send_utf8($pintable);
		}
		$last_command_status = "PIN $pinid / $$rmsg{'data'}{'pinname'} saved.";
		print Dumper $statemessage_src;
		my $mstatemessage = $json->encode( $statemessage_src->() );
		print"Sending: $mstatemessage\n";
		$conn->send_utf8($mstatemessage);
	}
}

#========================== B E G I N =====================================

if ( defined $configfile ) {
	if ( -r $configfile) {
		open ($filehandle, "$configfile") || die "Plan_seq: Can't open $configfile: $!\n";
		read_config ( $filehandle );
		close ( $filehandle );

	} else {
		die "Responder: Can't find $configfile!\n";
	}
} else {
	@knownPins	= 	(	['1','Humpal','12689567','2026-03-11','2026-03-15','5',],
						['2','Kule','49786543','2026-02-10','2026-04-01','2'],
						['5','Fistula','18796654','0001-01-01','2026-04-16','-1'],
						['9','Ocicko','46992459','0001-01-01','9999-12-31','-1'],
					);
}

if ( defined $codefile ) {
	if ( -r $codefile) {
		open ($filehandle, "$codefile") || die "Plan_seq: Can't open $configfile: $!\n";
		read_code ( $filehandle );
		close ( $filehandle );

	} else {
		die "Responder: Can't find $codefile!\n";
	}
} else {
	@knownCodes	= 	(	['1','Tleskac','HZ1268956754M','2026-03-11','2026-03-15',],
						['2','Rychlonozka','DR4449786543LE','2026-02-10','2026-04-01',],
						['7','Dusin','PLafDz18796654001587','0001-01-01','2026-04-16',],
						['11','Cervenacek','DR4628247925E','0001-01-01','9999-12-31',],
					);
}



#my $json = JSON::MaybeXS->new(utf8 => 1, pretty => 0);

print"Web Socket server listening on ws://localhost:$port\n";
print "Available " . scalar@knownPins . "PINs, first unused: " . getlastpid() . "\n";
print "Available " . scalar@knownCodes . "codes, first unused: " . getlastcode() . "\n";

#my$ws;
$ws=Net::WebSocket::Server->new(
	listen=>$port,
	tick_period=>$tickperiod,
	on_tick=>sub {
		my ($serv) = @_;
        my $now = time();
		if ($now - $prevstat > 15) {					# status date
			$prevstat = $now;
		}
		if ( scalar @todolist > 0 ) {					# TODO list
			print "Something there\n";
			print "Todo head: " . $todolist[0][0] . " / $now\n";
			while (scalar @todolist > 0 && $todolist[0][0] <= $now) {
				my $action = $todolist[0][1];
				print "Action to perform: $action\n";
				if ( $action eq 'closedoor' ) {
					$door_state_open = 'no';
					$door_state_closed = 'yes';
					my $mdoor = $json->encode($door_src->());
					print"Sending: $mdoor\n";
					foreach($ws->connections()){
						$_->send_utf8($mdoor);
					}
				}
				if ( $action eq 'ambientoff' ) {
					$ambient_state_off = 'yes';
					$ambient_state_on = 'no';
					my $mambient = $json->encode($ambient_src->());
					print"Sending: $mambient\n";
					foreach($ws->connections()){
						$_->send_utf8($mambient);
					}
				}
				shift @todolist;
			}
		}
		if ($now - $prevping > $watchdogperiod) {						# watchdog
			$prevping = $now;
			for my $conn ($serv->connections) {
				# Disconnect if no pong received in last 30 seconds
				if (exists $last_pong{$conn} && $now - $last_pong{$conn} > $deadtimeout) {
					print "Client $conn seems dead, dropping\n";
					$conn->disconnect();
					next;
				}
				print "Watchdog for $conn \n";
				$conn->send_utf8('{"_timestamp_":"' . time() . '","_command_":"_ping_","data":{}}');
			}
		}
		
	},
	on_connect=>sub{										#incomming messages
		my($serv,$conn)=@_;
		print"Client connected\n";
		$conn->on(
			utf8=>sub{
				my($conn,$msg)=@_;
				my $now = time();
				print"Received:$msg\n";
				my $rmsg = $json->decode($msg);
				unless ( defined $$rmsg{'_command_'} ) {
					print "MALFORMED - discarding!\n";
					return;
				}
				my $rcommand = $$rmsg{'_command_'};
				print "Command decoded: $rcommand\n";
				$last_pong{$conn} = time();
				if($rcommand eq 'opendoor'){
					$door_state_open = 'yes';
					$door_state_closed = 'no';
					$ambient_state_on = 'yes';
					$ambient_state_off = 'no';
					push (@todolist, [$now + $dooropentime, 'closedoor']);
					push (@todolist, [$now + $dooropentime + $ambientonoverhang, 'ambientoff']);
#					print Dumper @todolist;
					my $mmessage = $json->encode($openboxmessage_src->());
					print"Sending: $mmessage\n";
					foreach($ws->connections()){
						$_->send_utf8($mmessage);
					}
				}
				if($rcommand eq 'get_door' ){
					my $mdoor = $json->encode($door_src->());
					print"Sending: $mdoor\n";
					$conn->send_utf8($mdoor);
				}
				if($rcommand eq 'get_ambient' ){
					my $mambient = $json->encode($ambient_src->());
					print"Sending: $mambient\n";
					$conn->send_utf8($mambient);
				}
				if($rcommand eq 'get_creds'){
					my $credType = $$rmsg{'data'}{'credType'};
					if ($credType eq 'ctPin' ) {
						my $pintable = $json->encode( $pintable_src->() );
						print"Sending: $pintable\n";
						$conn->send_utf8($pintable);
	#					my $lmsg = '{"_timestamp_":"78960887635","_command_":"c_content","data":{"pinrows":"' . getPins() . '"}}';
	#					print"Sending: $lmsg\n";
	#					$conn->send_utf8($lmsg);
					}
					if($credType eq 'ctCode') {
						my $codetable = $json->encode( $codetable_src->() );
						print"Sending: $codetable\n";
						$conn->send_utf8($codetable);
	#					my $lmsg = '{"_timestamp_":"78960887635","_command_":"c_content","data":{"pinrows":"' . getPins() . '"}}';
	#					print"Sending: $lmsg\n";
	#					$conn->send_utf8($lmsg);
					}
				}
				if ( $rcommand eq 'set_cred' ) {
					if($$rmsg{'data'}{'credType'} eq 'ctPin' ){
						set_pin( $rmsg, $conn );
					} elsif ($$rmsg{'data'}{'credType'} eq 'ctCode' ){
						set_code ( $rmsg, $conn );
					} else {
						print "Unknown cred type used!\n";
					}
				}

			},
			disconnect=>sub{
				print"Client disconnected\n";
				delete $last_pong{$conn};
			},
		);
		$last_pong{$conn} = time();
	},
);
$ws->start;
print "WS started!";