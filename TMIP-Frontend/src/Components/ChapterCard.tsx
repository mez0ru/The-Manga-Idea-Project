import { useState } from 'react'
import { BASE_URL } from '../api/axios';
import Card from '@mui/material/Card';
import CardContent from '@mui/material/CardContent';
import CardMedia from '@mui/material/CardMedia';
import Typography from '@mui/material/Typography';
import CardActionArea from '@mui/material/CardActionArea';
import { Chapter } from '../ChaptersPage';
import Grow from '@mui/material/Grow';
import { Link } from 'react-router-dom';

interface Props {
    chapter: Chapter;
}

export default function SeriesCard({ chapter }: Props) {
    const [done, setDone] = useState(false);

    return (
        <Grow in={done}>
            <Card sx={{ maxWidth: 150 }}>
                <CardActionArea component={Link} to={`/series/${chapter.seriesId}/chapter/${chapter.id}`}>
                    <CardMedia
                        component="img"
                        alt={chapter.name}
                        onLoad={() => setDone(true)}
                        onError={() => setDone(true)}
                        src={`${BASE_URL}/api/chapter/${chapter.id}/cover`}
                    />
                    <CardContent>
                        <Typography gutterBottom variant="body1" sx={{
                            overflow: "hidden",
                            textOverflow: "ellipsis",
                            display: "-webkit-box",
                            WebkitLineClamp: "2",
                            WebkitBoxOrient: "vertical",
                        }}>
                            {chapter.name}
                        </Typography>
                        <br />
                        <Typography gutterBottom variant="body2">
                            {chapter.pages} Pages
                        </Typography>
                    </CardContent>
                </CardActionArea>
            </Card>
        </Grow>
    )
}
