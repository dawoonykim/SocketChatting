drop database if exists chatting;

create database chatting default character set utf8mb4 collate utf8mb4_unicode_ci;

use chatting;

drop table if exists users;
create table users(
	user_id int auto_increment primary key,
    username varchar(50) not null unique,
    password varchar(100) not null,
    status enum("active","suspended") default "active",
    created_at datetime default current_timestamp
);

drop table if exists message_log;
create table message_log(
	message_id int auto_increment primary key,
    sender_id int,
    content text,
    sent_at datetime default current_timestamp,
    
    constraint fk_message_id
    foreign key (sender_id) references users(user_id)
		on delete cascade
);

drop table if exists user_sessions;
create table user_sessions(
	session_id int auto_increment primary key,
    user_id int,
    login_time datetime default current_timestamp,
    logout_time datetime,
    ip_address varchar(100),
    
    constraint fk_session_id
    foreign key (user_id) references users(user_id)
		on delete cascade
);

-- Test user information
insert into users (username, password) values("asdf","asdf");

select * from users;
select * from message_log;
select * from user_sessions;