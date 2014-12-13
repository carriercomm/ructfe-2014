package Monitor;
use Mojo::Base 'Mojolicious';

use Mojo::Util 'slurp';
use Mojo::Pg;
use Time::Piece;

has services   => sub { {} };
has teams      => sub { {} };
has scoreboard => sub { [] };
has round      => sub { {} };
has status     => sub { {} };
has flags      => sub { {} };
has history    => sub { [] };

has ip2team => sub { {} };

sub startup {
  my $app = shift;

  my $mode = $app->mode;
  $app->plugin('Config', file => "monitor.$mode.conf");

  $app->helper(
    pg => sub {
      state $pg = Mojo::Pg->new($app->config->{db});
    });

  my $r = $app->routes;
  $r->get('/')->to('main#index')->name('index');
  $r->get('/flags')->to('main#flags')->name('flags');
  $r->get('/history')->to('main#history')->name('history');

  $app->log->info('Fetch services at startup');
  $app->pg->db->query(
    'SELECT id, name FROM services',
    sub {
      my ($db, $err, $res) = @_;
      return $app->log->error("Error while select services: $err") if $err;

      $res->arrays->map(sub { $app->services->{$_->[0]} = $_->[1] });
    });

  $app->log->info('Fetch teams at startup');
  $app->pg->db->query(
    'SELECT id, name, vuln_box FROM teams',
    sub {
      my ($db, $err, $res) = @_;
      return $app->log->error("Error while select services: $err") if $err;

      $res->hashes->map(
        sub {
          $app->teams->{$_->{id}}         = $_;
          $app->ip2team->{$_->{vuln_box}} = $_->{name};
        });
    });

  Mojo::IOLoop->recurring(
    30 => sub {
      $app->log->info('Update scoreboard');
      Mojo::IOLoop->delay(
        sub {
          my $delay = shift;

          $app->pg->db->query(
            'SELECT DISTINCT ON (team_id, service_id) team_id, service_id, score
            FROM score ORDER BY team_id, service_id, time DESC'
              => $delay->begin
          );
          $app->pg->db->query(
            'SELECT DISTINCT ON (team_id, service_id) team_id, service_id, successed, failed
            FROM sla ORDER BY team_id, service_id, time DESC'
              => $delay->begin
          );
          $app->pg->db->query(
            'SELECT n, EXTRACT(EPOCH FROM time) AS time
            FROM rounds ORDER BY n DESC LIMIT 1'
              => $delay->begin
          );
          $app->pg->db->query(
            'SELECT team_id, service_id, status, fail_comment FROM service_status' =>
              $delay->begin);
          $app->pg->db->query('SELECT * FROM services_flags_stolen' => $delay->begin);
        },
        sub {
          my ($delay, $e1, $scores, $e2, $services, $e3, $round, $e4, $status, $e5, $flags) = @_;
          if (my $e = $e1 || $e2 || $e3 || $e4 || $e5) {
            $app->log->error("Error while update scoreboard: $e");
            return;
          }

          my ($flag_points, $sla_points);

          $services->hashes->map(
            sub {
              my ($sla, $sum) = (1, $_->{successed} + $_->{failed});
              $sla = $_->{successed} / $sum if $sum > 0;
              $sla_points->{$_->{team_id}}{$_->{service_id}} = $sla;
            });

          $scores->hashes->map(
            sub {
              $flag_points->{$_->{team_id}}{$_->{service_id}} = $_->{score};
            });

          my @data;
          for my $tid (keys %{$app->teams}) {

            my $score = 0;
            $score += $sla_points->{$tid}{$_} * $flag_points->{$tid}{$_} for keys %{$app->services};

            push @data, {
              team => {
                id       => $tid,
                name     => $app->teams->{$tid}{name},
                vuln_box => $app->teams->{$tid}{vuln_box}
              },
              sla   => $sla_points->{$tid},
              fp    => $flag_points->{$tid},
              score => $score
              };
          }

          @data = sort { $b->{score} <=> $a->{score} } @data;
          $app->scoreboard(\@data);

          my $r = $round->hash;
          $app->round({n => $r->{n}, time => scalar localtime int $r->{time}});

          $status->hashes->map(
            sub {
              $app->status->{$_->{service_id}}{$_->{team_id}} = $_;
            });

          $flags->hashes->map(
            sub {
              $app->flags->{$_->{team_id}}{$_->{service_id}} =
                {count => $_->{flags}, name => $_->{service}};
            });
        });
    });

  Mojo::IOLoop->recurring(
    120 => sub {
      $app->log->info('Update history');
      $app->pg->db->query(
        'SELECT * FROM points_history
        ORDER BY team_id, round' => sub {
          my ($db, $err, $res) = @_;
          return $app->log->error("Error while select services: $err") if $err;

          my ($h, $nh);
          $res->hashes->map(
            sub {
              if (($_->{round} > $app->round->{n} - 40) or ($_->{round} % 15 == 0)) {
                push @{$h->{$_->{name}}}, {x => $_->{round}, y => 0 + $_->{points}};
              }
            });

          for (keys %$h) {
            push @$nh, {name => $_, data => $h->{$_}};
          }
          $app->history($nh);
        });
    });
}

1;
