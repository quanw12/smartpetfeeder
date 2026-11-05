-- ĐỀ XUẤT, cần Thuận (ID 3) chốt. ID 4 và ID 8 đọc đúng tên bảng và tên cột dưới đây.
-- Chạy trong Supabase -> SQL Editor.

create table if not exists public.feeder_events (
  id          bigint generated always as identity primary key,
  created_at  timestamptz not null default now(),
  event       text not null check (event in ('level', 'feed')),
  level       integer check (level between 0 and 100),   -- event = 'level'
  duration_ms integer check (duration_ms > 0)            -- event = 'feed'
);

create index if not exists feeder_events_event_time
  on public.feeder_events (event, created_at desc);

-- Chặn truy cập bằng anon key. Node-RED dùng service_role key nên vẫn đọc ghi được.
alter table public.feeder_events enable row level security;
