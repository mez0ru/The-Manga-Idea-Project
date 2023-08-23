import { ReactElement, useEffect, useRef, useState } from 'react'
import { BASE_URL } from '../api/axios';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import Typography from '@mui/material/Typography';
import CardActionArea from '@mui/material/CardActionArea';
import { Chapter } from '../ChaptersPage';
import Grow from '@mui/material/Grow';
import { Link } from 'react-router-dom';
import './ChapterCard.css'
import { useChaptersStore } from '../store/ChaptersStore';

interface Props {
    chapter: Chapter;
    index: number;
    animate: boolean;
}

interface GrowProps {
    children: ReactElement<any, any>;
    done: boolean;
    animate: boolean;
}

const GrowComponent = ({ children, done, animate }: GrowProps) => {
    if (animate) {
        return (<Grow in={done}>
            {children}
        </Grow>)
    } else {
        return <>{children}</>
    }
}
export default function SeriesCard({ chapter }: Props) {
    const [done, setDone] = useState(false);

    const selectedChapter = useChaptersStore((state) => state.selectedChapter);
    const refThis = useRef<HTMLDivElement>(null);

    if (chapter.id === selectedChapter) {
        useEffect(() => {
            const scrollIntoViewWithOffset = (selector: HTMLDivElement, offset: number) => {
                document.body.scrollTo({
                    behavior: 'smooth',
                    top:
                        selector.getBoundingClientRect().top -
                        document.body.getBoundingClientRect().top -
                        offset,
                })
            }


            if (refThis.current) {
                refThis.current?.scrollIntoView({ behavior: 'smooth', block: 'end' })
            }
        }, [refThis])
    }

    return (
        <GrowComponent done={done} animate={selectedChapter === -1}>
            <Card sx={{ maxWidth: 160 }} className={selectedChapter === chapter.id ? 'highlight' : ''} ref={refThis}>
                <CardActionArea component={Link} to={`/series/${chapter.seriesId}/chapter/${chapter.id}`}>
                    <CardMedia
                        component="img"
                        alt={chapter.name}
                        onLoad={() => setDone(true)}
                        onError={() => setDone(true)}
                        src={`${BASE_URL}/api/v2/chapter/cover/${chapter.id}`}
                        style={{ height: '215px' }}
                    />
                    <CardContent>
                        <Typography gutterBottom variant="body1" sx={{
                            overflow: "hidden",
                            textOverflow: "ellipsis",
                            display: "-webkit-box",
                            WebkitLineClamp: "2",
                            WebkitBoxOrient: "vertical",
                            minHeight: '4.5rem',
                            marginBottom: '0.5rem',
                        }}>
                            {chapter.name}
                        </Typography>
                        <Typography gutterBottom variant="body2">
                            {chapter.pages_count} Pages
                        </Typography>
                    </CardContent>
                </CardActionArea>
            </Card>
        </GrowComponent>
    )
}
