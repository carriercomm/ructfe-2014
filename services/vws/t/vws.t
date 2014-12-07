use FindBin '$Bin';
use IPC::Run 'start';
use Mojo::IOLoop::Server;
use Mojo::URL;
use Test::Mojo;
use Test::More;

use warnings;
use strict;

my ($port, $vws);
BEGIN {
  $port = Mojo::IOLoop::Server->generate_port;
  $vws = start ["$Bin/../vws", '-p', $port, '-i', '1', '-d', "$Bin/static"];
  unlink "$Bin/static/data";
  sleep 1;
}
END { $vws->signal('SIGTERM') }

my $url = Mojo::URL->new("http://localhost:$port/");
my $t   = Test::Mojo->new->tap(sub { $_->ua->max_connections(0) });

# Headers
$t->get_ok($url->path('/1.txt'))
  ->status_is(200)
  ->header_is(Server => 'VWS')
  ->header_is('X-Powered-By' => 'Vala 0.26.0')
  ->header_is('Content-Length' => 10)
  ->content_is("test data\n");

$t->get_ok($url->path('/-1.txt'))
  ->status_is(404)
  ->header_is(Server => 'VWS')
  ->header_is('X-Powered-By' => 'Vala 0.26.0');

# PUT
my $data = "test\ndata\nfor\n\nput\n";
$t->put_ok($url->path('/data') => $data)->status_is(200);
$t->get_ok($url->path('/data'))
  ->status_is(200)
  ->header_is('Content-Length' => length $data)
  ->content_is($data);

# Path
$t->get_ok($url->path('///1.txt'))->status_is(200);
$t->get_ok($url->path('/1.txt/../1.txt'))->status_is(200);
$t->get_ok($url->path('/a/b/../../1.txt'))->status_is(200);
$t->get_ok($url->path('/../../1.txt'))->status_is(200);
$t->get_ok($url->path('/a/b/c/../../1.txt'))->status_is(404);

# Test vuln
$t->get_ok($url->path('/../vws.t'))->status_is(404);
$t->get_ok($url->path('/%2E%2E/vws.t'))->status_is(200);

# Erros
$t->post_ok($url->path('/'))->status_is(501);
$t->options_ok($url->path('/'))->status_is(501);

done_testing();
